# Work Log

## 2026-08-01
- `set` is an expression now, so assignment works in a condition
  (`(while (!= (set x (getchar)) EOF) ...)`) and chains
- The op-assigns match it: expressions too, and they take any lvalue
  instead of only a bare name
- sic-mode: fixed `|` inheriting lisp's string-quote syntax (a bitwise
  or opened a string), and gave operator heads their own face so Emacs
  and the tree-sitter queries agree on what a head is
- Stopped emitting `if ((a == b))`; conditions no longer double the
  operator's parens, which clang rejected under -Werror all along

## 2026-07-29
- Emacs sic-mode: font-lock, indentation, imenu, completion, flymake
  (piggybacking on the #line markers for C diagnostics)
- sic-lsp: LSP proxy putting clangd behind .sic buffers (completion,
  hover, definitions, references) via the generated C and #line map
- Tree-sitter grammar + highlight queries for non-Emacs editors
- Rejected extra declaration forms instead of silently ignoring them
- Made runtime goldens check exit status and exact stdout bytes
- Made comparisons explicitly binary instead of inheriting C's chained semantics
- Fixed undefined `va_list` reuse while formatting generated C
- CUDA support: launch form for kernel calls (qualifiers already worked
  via hyphen-types), saxpy example, codegen golden tests + nvcc/GPU
  gated test tiers
- Template macros: defmacro with rest-param splicing, `#` auto-gensym,
  outermost-first expansion pass between parse and transpile
- Makefile + golden test harness (incl. error-case tests), bug fixes
- Line comments, positioned diagnostics, single-line C output
- Language completeness: full operator set, if/do/?:, aref/->/.,
  generalized set, hyphenated :types, struct/union/enum/typedef,
  switch/do-while/goto, bare return
- README language reference; DESIGN.md records decisions

## 2025-05-10
- Outlined general plan
- Bought the domain for `siclang.com`

## 2025-05-09
- Handrolled transpiler rules
- Generated a first working C program!

## 2025-05-08
- Add parser abstraction, simple atoms / sexps

## 2025-05-04
- Kick things off
