// Wisp (SRFI-119) external scanner: indentation structure.
//
// Emits zero-width _indent/_dedent/_newline tokens from the indentation
// of physical lines, mirroring the reference implementation (wisp.py in
// the wisp repository), plus the visible line_prefix token. The
// underscore_indent token is *not* emitted here: measuring the next
// line's indentation consumes the underscore run (it counts as columns),
// and there is no way to rewind within a scanner call, so the scanner
// could never see the run again. Instead the generated lexer matches it
// as a regular token (_+ followed by a literal space), which is only
// valid at line starts, where longest-match beats `atom`.
//
// Architecture (the tree-sitter-python model, simplified):
//
//   - scan() is called when any external token is valid. It marks the
//     token end at the current position first; on a `true` return the
//     lexer rewinds to that position, so zero-width tokens and all
//     "skipped" lookahead are re-lexed by the generated lexer
//     afterwards. Measurement is pure peek.
//
//   - At a '\n' the scanner peeks forward, skipping blank lines and
//     ';'-comment lines, and measures the indentation of the next code
//     line. Then it emits, in order of priority:
//       1. NEWLINE, if the parser still expects one (the grammar has no
//          bare newline token, so after a line consumes its newline,
//          NEWLINE stops being valid and this naturally terminates);
//       2. DEDENT, if the next line is shallower than the stack top
//          (one level per token, the stack itself tracks progress);
//       3. INDENT, if the next line is deeper;
//       4. line_prefix, if the next line starts with a prefix (and not
//          with an underscore-indent run — the prefix must follow the
//          run, which the generated lexer produces). The prefix is
//          emitted from the boundary because tree-sitter excludes
//          external tokens from reduce lookaheads, so after the
//          previous line reduces the lexer may never consult the
//          scanner at the line start itself.
//     Otherwise it returns false and the generated lexer continues.
//
//   - Blank lines and ';'-comment lines therefore contribute no tokens
//     but are re-lexed as whitespace/comment extras (comment nodes are
//     preserved). They never affect the indentation stack, matching
//     wisp.py.
//
// Indentation of a logical line is its leading whitespace, with a run of
// '_' at the very start of the line counting as whitespace when followed
// by a space or the end of the line ("___ x" is indented 4 columns);
// '\' escapes that substitution ("___foo" is content at column 0).
//
// Because wisp disables itself inside brackets and strings, the generated
// lexer absorbs all newlines inside paren/bracket/curly/vector/string
// tokens; the scanner is never consulted there and therefore needs no
// bracket tracking.
//
// Deviations from the reference implementation (documented in README):
//   - '#|' block-comment lines count as code lines for the indentation
//     stack (they contribute no content tokens, so they cancel out);
//   - a dedent to an indentation level that was never defined is repaired
//     by opening a new block at that level instead of erroring;
//   - leading indentation on the very first line is ignored;
//   - a line consisting only of underscores parses as a symbol line
//     (the reference implementation treats it as a blank line).

#include "tree_sitter/alloc.h"
#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Order must match the `externals` array in grammar.js.
enum TokenType {
  INDENT,
  DEDENT,
  NEWLINE,
  LINE_PREFIX,
};

// Serialized scanner state must fit TREE_SITTER_SERIALIZATION_BUFFER_SIZE
// (1024 bytes): 480 levels * 2 bytes + depth + one flag byte = 963 bytes.
#define MAX_DEPTH 480

typedef struct {
  uint16_t indent_stack[MAX_DEPTH];
  uint16_t depth;
  // Synthesized end-of-file NEWLINE already emitted (the last line of a
  // file may lack its terminating newline, which the grammar requires).
  bool eof_newline_emitted;
} Scanner;

void *tree_sitter_wisp_external_scanner_create(void) {
  Scanner *s = ts_calloc(1, sizeof(Scanner));
  // Level 0 (the top level) is always on the stack.
  s->depth = 1;
  return s;
}

void tree_sitter_wisp_external_scanner_destroy(void *payload) {
  ts_free(payload);
}

unsigned tree_sitter_wisp_external_scanner_serialize(void *payload,
                                                     char *buffer) {
  memcpy(buffer, payload, sizeof(Scanner));
  return sizeof(Scanner);
}

void tree_sitter_wisp_external_scanner_deserialize(void *payload,
                                                   const char *buffer,
                                                   unsigned length) {
  memcpy(payload, buffer, length);
}

static void skip(TSLexer *lexer) { lexer->advance(lexer, true); }

// If the current position starts a line prefix followed by a literal
// space (as in the reference implementation), consume it into a
// LINE_PREFIX token and return true. The candidates are matched with
// at most three consumed characters; on mismatch the anchor rewind
// restores the position.
static bool try_line_prefix(TSLexer *lexer) {
  int32_t c = lexer->lookahead;

  if (c == '\'' || c == ',' || c == '`') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == ' ') {
      lexer->mark_end(lexer);
      lexer->result_symbol = LINE_PREFIX;
      return true;
    }
    return false; // e.g. 'foo: the prefix is not a prefix; re-lex as atom
  }

  if (c == '#') {
    lexer->advance(lexer, false);
    int32_t d = lexer->lookahead;
    if (d == '\'' || d == '`' || d == '@') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == ' ') {
        lexer->mark_end(lexer);
        lexer->result_symbol = LINE_PREFIX;
        return true;
      }
      return false;
    }
    if (d == ',') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '@') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == ' ') {
          lexer->mark_end(lexer);
          lexer->result_symbol = LINE_PREFIX;
          return true;
        }
        return false;
      }
      if (lexer->lookahead == ' ') {
        lexer->mark_end(lexer);
        lexer->result_symbol = LINE_PREFIX;
        return true;
      }
      return false;
    }
    return false;
  }

  return false;
}

// Peek forward from the current '\n': skip blank lines, ';'-comment
// lines, and leading whitespace of the next code line, and record that
// line's indentation. All skipping is undone by the mark_end rewind.
// *has_underscore_run reports whether the next code line starts with an
// underscore-indent run (the run counted as indentation); the scanner
// must then not emit a line_prefix, because the run itself is lexed as
// an underscore_indent token by the generated lexer and the prefix must
// follow it.
static void measure_next_line(TSLexer *lexer, uint16_t *indent,
                              bool *found, bool *has_underscore_run) {
  *found = false;
  *has_underscore_run = false;

  for (;;) {
    // End of the previous line.
    if (lexer->lookahead == '\n') {
      skip(lexer);
    } else if (lexer->eof(lexer)) {
      return;
    }

    uint32_t columns = 0;
    *has_underscore_run = false;

    // A run of '_' at the very start of a line counts as whitespace when
    // followed by a space or the end of the line, exactly as in wisp.py
    // ("___ x" = 4 columns, "___foo" is content at column 0).
    if (lexer->lookahead == '_') {
      uint32_t run = 0;
      while (lexer->lookahead == '_') {
        run++;
        skip(lexer);
      }
      if (lexer->lookahead == ' ' || lexer->lookahead == '\n' ||
          lexer->eof(lexer)) {
        columns += run;
        // Only a run followed by a literal space produces an
        // underscore_indent token; a run at the end of the line makes
        // the line blank, and a run followed by content is an atom.
        *has_underscore_run = (lexer->lookahead == ' ');
      } else {
        // The underscores are content: an unindented code line.
        *indent = 0;
        *found = true;
        return;
      }
    }

    for (;;) {
      if (lexer->lookahead == ' ' || lexer->lookahead == '\t' ||
          lexer->lookahead == '\r') {
        columns++;
        skip(lexer);
      } else {
        break;
      }
    }

    if (lexer->lookahead == '\n' || lexer->eof(lexer)) {
      continue; // blank line: no structural weight
    }

    if (lexer->lookahead == ';') {
      // Comment-only line: no structural weight; skip to its end.
      while (lexer->lookahead != '\n' && !lexer->eof(lexer)) {
        skip(lexer);
      }
      continue;
    }

    // A code line.
    *indent = (uint16_t)columns;
    *found = true;
    return;
  }
}

static bool scan(Scanner *s, TSLexer *lexer, const bool *valid_symbols) {
  // Anchor: the token end and the rewind point for everything skipped
  // while measuring.
  lexer->mark_end(lexer);

  if (lexer->eof(lexer)) {
    // The last line of the file may lack its terminating newline; the
    // grammar requires one, so synthesize it. It must come before any
    // pending dedents: a line needs its newline before its block can
    // close.
    if (valid_symbols[NEWLINE] && !s->eof_newline_emitted) {
      s->eof_newline_emitted = true;
      lexer->result_symbol = NEWLINE;
      return true;
    }
    if (valid_symbols[DEDENT] && s->depth > 0) {
      s->depth--;
      lexer->result_symbol = DEDENT;
      return true;
    }
    return false;
  }


  // Direct line-start check: the first line of the file (and lines
  // reached through an INDENT) may start with a prefix.
  if (valid_symbols[LINE_PREFIX] && try_line_prefix(lexer)) {
    return true;
  }

  if (lexer->lookahead == '\n') {
    // A line still needs its terminating newline first.
    if (valid_symbols[NEWLINE]) {
      lexer->result_symbol = NEWLINE;
      return true;
    }

    uint16_t next_indent;
    bool found;
    bool has_underscore_run;
    measure_next_line(lexer, &next_indent, &found, &has_underscore_run);
    if (!found) {
      // Only blank/comment lines or EOF follow: the file ends here, so
      // every open block must close. The DEDENTs are emitted one per
      // call from this position (the stack tracks progress); the EOF
      // branch below handles the same flush when the last line lacks a
      // trailing newline and the scanner is consulted at EOF itself.
      if (valid_symbols[DEDENT] && s->depth > 0) {
        s->depth--;
        lexer->result_symbol = DEDENT;
        return true;
      }
      return false;
    }

    if (s->depth > 0 && next_indent < s->indent_stack[s->depth - 1]) {
      if (valid_symbols[DEDENT]) {
        s->depth--;
        lexer->result_symbol = DEDENT;
        return true;
      }
      return false;
    }

    if (s->depth > 0 && next_indent > s->indent_stack[s->depth - 1]) {
      if (valid_symbols[INDENT]) {
        if (s->depth < MAX_DEPTH) {
          s->indent_stack[s->depth++] = next_indent;
          lexer->result_symbol = INDENT;
          return true;
        }
      }
      return false;
    }

    // Same indentation (or leading indentation at depth 0). The next
    // line may start with a line prefix; emit it here, because external
    // tokens are excluded from reduce lookaheads: once the previous
    // line reduces, the lexer may never consult the scanner at the line
    // start itself.
    //
    // An underscore-indent run is lexed as a regular token by the
    // generated lexer (the scanner cannot emit it: measuring the next
    // line's indentation consumes the run, and there is no rewind). If
    // the next line starts with such a run, leave the prefix for a
    // later scanner call, after the parser has consumed the run.
    if (has_underscore_run) {
      return false;
    }
    if (valid_symbols[LINE_PREFIX] && try_line_prefix(lexer)) {
      return true;
    }
    return false;
  }

  return false;
}

bool tree_sitter_wisp_external_scanner_scan(void *payload, TSLexer *lexer,
                                            const bool *valid_symbols) {
  return scan(payload, lexer, valid_symbols);
}
