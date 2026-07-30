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
