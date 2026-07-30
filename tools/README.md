# Editor tooling

## Emacs: sic-mode.el

A major mode deriving from `lisp-data-mode`, so paredit, smartparens,
and rainbow-delimiters work out of the box. It adds font-lock for the
form heads in `TRANSPILE_RULES`, `:type` atoms, and definition names;
two-space body indentation (customize `sic-mode-indent-rules` for your
own defmacro control forms); imenu over `fn`/`defmacro`/type
definitions; completion-at-point over builtins, `:types`, and symbols
from open (sic) buffers; and a flymake backend.

```elisp
(use-package sic-mode
  :load-path "~/dev/sic/tools"
  :mode "\\.sic\\'"
  :hook (sic-mode . flymake-mode))
```

The flymake backend runs `sicc` on the buffer and then `cc -Wall
-Wextra -fsyntax-only` on the generated C. Because the generated C
carries `#line` markers pointing back at the source, the C compiler
reports errors at `.sic` positions on its own — sicc diagnostics and C
type errors both land on the right line with no mapping layer. `sicc`
is found in a dominating directory of the file (so working inside this
repo just works), or set `sic-mode-sicc-program`. For `.cu.sic`
buffers the C stage uses nvcc when available and is skipped otherwise.

## LSP: sic-lsp

`sic-lsp` is a proxy that puts clangd behind `.sic` files: semantic
completion, hover, go-to-definition (including into C headers),
find-references, signature help, and live clang diagnostics. Each
buffer is retranspiled on change; clangd analyzes the generated C and
the proxy translates positions both ways through the `#line` map. See
DESIGN.md ("Editor tooling") for how.

Requires `clangd` and `python3` (stdlib only). `sicc` is found via
`--sicc`, `$SICC`, `<workspace root>/sicc`, then `$PATH` — so opening
files inside this repo just works once `make` has run.

```elisp
;; Emacs (eglot)
(with-eval-after-load 'eglot
  (add-to-list 'eglot-server-programs
               '(sic-mode . ("~/dev/sic/tools/sic-lsp"))))
M-x eglot
```

Notes:
- eglot replaces the flymake backend while connected; diagnostics then
  come from the proxy (sicc errors + clang warnings), which is a
  superset of what `sic-flymake` reports.
- Completion works best with parens kept balanced (paredit,
  electric-pair-mode): `(. s fi|)` transpiles to `(s).fi`, so clangd
  sees a member access and completes struct fields.
- Any LSP client works the same way, e.g. Neovim:
  `vim.lsp.start { cmd = { '/path/to/sic/tools/sic-lsp' } }` for
  `*.sic` buffers.

## Tree-sitter: tree-sitter-sic/

A grammar for editors beyond Emacs (Neovim, Zed, Helix) and anything
else tree-sitter reaches. Atoms are whitespace/paren-delimited, so the
grammar is one regex per atom category plus lists;
`queries/highlights.scm` classifies form heads (keywords, builtins,
operators, definition names). The generated parser is not checked in:

```sh
cd tools/tree-sitter-sic
npx tree-sitter-cli@0.25 generate   # writes src/parser.c
npx tree-sitter-cli@0.25 test       # corpus tests
```

In Emacs 29+ the plain `sic-mode` already covers highlighting; the
grammar exists for portability, not because Emacs needs it.
