; Wisp (SRFI-119) highlighting.
;
; The layout-first tree has no semantic list nodes: the first datum of a
; line is its head (the called procedure), the first datum of a colon_list
; is the head of the colon group.

(comment) @comment
(block_comment) @comment
(shebang) @comment

(string) @string
(char) @character
(number) @number

(line_prefix) @operator
(colon_list ":" @operator)
(continuation "." @operator)

; Well-known special forms and builtins first, so they win over the
; generic head-of-line capture below.
((atom) @keyword
 (#any-of? @keyword
  "define" "define*" "define-syntax" "define-syntax-rule" "define-values"
  "lambda" "lambda*"
  "let" "let*" "letrec" "letrec*" "let-values" "let*-values"
  "if" "cond" "case" "else" "=>"
  "and" "or" "not"
  "when" "unless"
  "begin" "begin0"
  "set!" "set!-values"
  "quote" "quasiquote" "unquote" "unquote-splicing"
  "syntax-rules" "syntax-case" "syntax-error"
  "do" "delay" "delay-force" "make-promise"
  "call/cc" "call-with-current-continuation"
  "car" "cdr" "cons" "list" "apply" "map" "fold" "reduce"
  "display" "newline" "write" "read" "eval" "load"
  "assert" "pk" "values"))

; Constants.
((atom) @constant (#match? @constant "^#[tf]$"))

; First datum of a line: the procedure being called.
(line head: (atom) @function.call)
(colon_list . (atom) @function.call)

; Everything else is a variable or symbol.
(atom) @variable
