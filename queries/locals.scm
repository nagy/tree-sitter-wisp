; Wisp (SRFI-119) locals.
;
; Wisp has no lexical declaration syntax: like Scheme, bindings are
; introduced by macros (define, let, lambda, ...) which no tree query
; can interpret. Definition/reference analysis therefore cannot be
; expressed as a locals query; this file exists so tooling can ship the
; language with a complete query set.

; Scope: every indented block is a lexical region; its enclosing line's
; head and colon-group heads are in scope there.
(block) @scope
