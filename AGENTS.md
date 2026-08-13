# tree-sitter-wisp — project conventions

## What this is

A tree-sitter grammar for Wisp (SRFI-119: whitespace-to-lisp Scheme).
Layout-first concrete syntax tree: every source construct is a node
(`line`, `block`, `continuation`, `colon_list`, `paren_list`, ...).
The S-expression view is recovered via queries or a tree walk — never
the reverse. Grammar decisions are documented in README.org.

License: AGPL-3.0-or-later; the vendored reference test suite in
`test/wisp-suite/` stays expat-licensed (upstream wisp).

## Ground truth

- Spec: SRFI-119, https://srfi.schemers.org/srfi-119/srfi-119.html
- Reference implementation (the scanner mirrors its behavior):
  https://hg.sr.ht/~arnebab/wisp (`wisp.py`, test suite in `tests/`).
- `test/wisp-suite/` vendors the reference test suite (expat-licensed);
  keep it in sync when the upstream suite changes.

## Build & test

```sh
# Dev shell (tree-sitter CLI, nodejs, gcc): `nix develop`
tree-sitter generate   # src/parser.c, src/grammar.json, src/node-types.json
tree-sitter test       # runs test/corpus/*
test/parse-wisp-suite.sh  # parses every test/wisp-suite/*.w, must be error-free
```

Generated files (`src/parser.c`, `src/grammar.json`, `src/node-types.json`)
are committed. Never hand-edit them; regenerate after grammar.js changes.

`tree-sitter generate` must report zero conflicts. If a grammar change
introduces a conflict, fix it with `prec`/`conflicts` rather than
suppressing the report.

## Scanner rules (src/scanner.c)

- Emits zero-width `_newline`, `_indent`, `_dedent` external tokens,
  plus the visible `line_prefix` token (from the boundary call, because
  external tokens are excluded from reduce lookaheads).
- `underscore_indent` is deliberately NOT a scanner token: measuring the
  next line's indentation consumes the underscore run and there is no
  rewind within a scanner call. The generated lexer matches it (`_+`
  followed by a space; only valid at line starts).
- State = indentation stack (uint16 per level) + EOF-newline flag;
  serialization is a plain memcpy of the struct — do not add pointers.
- Blank and comment-only lines never affect the stack.
- `_`-indent (underscore run + whitespace at line start) counts columns
  like the reference implementation; `\` escapes it.
- Known deviations from wisp.py are listed in README.org; keep the list
  current when changing scanner behavior.

## Corpus tests

`test/corpus/*.txt` use the standard tree-sitter test format. When
adding a construct, add a corpus entry AND (where the reference suite
covers it) check the corresponding `test/wisp-suite/*.w` file parses
cleanly.

## Git

- Commits without GPG signing (`--no-gpg-sign`).
- Generated parser files are part of the repo; regenerate before commit
  if grammar.js changed.
