; Wisp (SRFI-119) indentation for editors that derive indentation from
; the tree (nvim-treesitter and similar).
;
; Wisp indentation is *written* in the source, so most editors should
; keep the source indentation instead of re-indenting. This query only
; describes the shape of blocks for tools that need it: children of a
; block belong to the line that owns the block.

(block) @indent
