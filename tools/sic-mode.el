;;; sic-mode.el --- Major mode for the (sic) language  -*- lexical-binding: t; -*-

;; Keywords: languages, lisp, c
;; Package-Requires: ((emacs "28.1"))

;;; Commentary:

;; Editing support for (sic), the s-expression language that transpiles
;; to C: font-lock, indentation, imenu, completion-at-point, and a
;; flymake backend that runs sicc plus a C compiler and maps C
;; diagnostics back to .sic lines through the #line markers sicc emits.
;;
;; See tools/README.md for setup, including LSP via tools/sic-lsp.

;;; Code:

(require 'lisp-mode)
(require 'flymake)

(defvar calculate-lisp-indent-last-sexp)

(defgroup sic nil
  "Editing support for the (sic) language."
  :group 'languages)

;; === Language surface ===

(defconst sic-mode--keywords
  '("fn" "decl" "set" "if" "do" "while" "do-while" "for" "switch" "case"
    "default" "return" "goto" "label" "struct" "union" "enum" "typedef"
    "defmacro" "launch" "fnptr")
  "Form heads drawn from sicc's TRANSPILE_RULES, plus macro forms.")

(defconst sic-mode--builtins
  '("init" "aref" "deref" "sizeof" "offsetof" "alignof")
  "Expression heads that read like operators rather than control flow.")

(defconst sic-mode--operators
  '("+" "-" "*" "/" "%" "<" ">" "<=" ">=" "==" "!=" "&&" "||" "&" "|" "^"
    "<<" ">>" "!" "~" "," "?:" "++" "--" "->" "."
    "+=" "-=" "*=" "/=" "%=" "&=" "|=" "^=" "<<=" ">>=")
  "Operator heads; kept in step with the operator pattern in
`tools/tree-sitter-sic/queries/highlights.scm'.  `set' is a keyword
in both tools, not an operator.")

(defconst sic-mode--preproc
  '("#include" "#define" "#undef" "#ifdef" "#ifndef" "#if" "#else" "#pragma")
  "Preprocessor form heads.")

(defconst sic-mode--types
  '(":int" ":char" ":short" ":long" ":float" ":double" ":void" ":bool"
    ":unsigned" ":unsigned-int" ":unsigned-long" ":unsigned-char"
    ":long-long" ":size_t" ":char*" ":const-char*" ":void*" ":int*"
    ":float*" ":double*")
  "Common types offered by completion; hyphen-types are open-ended.")

;; === Syntax ===

(defvar sic-mode-syntax-table
  (let ((table (make-syntax-table lisp-mode-syntax-table)))
    ;; Atoms may contain these: tmp#, :int[4], do-while, threadIdx.x,
    ;; (, a b), (?: c a b).  Make them symbol constituents so sexp
    ;; motion and \_< boundaries treat each atom as one unit.
    (dolist (c '(?# ?. ?, ?\[ ?\] ?? ?~))
      (modify-syntax-entry c "_" table))
    ;; lisp-mode-syntax-table makes | a string quote, for Common Lisp's
    ;; |symbols with spaces|.  sic has no such syntax and does have (| a
    ;; b), which would otherwise open a string that runs to the next |
    ;; or the end of the buffer.
    (modify-syntax-entry ?| "_" table)
    table))

(defconst sic-mode--syntax-propertize
  (syntax-propertize-rules
   ;; C character literals: make the quotes string delimiters so 'x'
   ;; and '\n' (and especially '"') don't derail string fontification.
   ("\\('\\)\\(?:\\\\.\\|[^'\\]\\)\\('\\)" (1 "\"") (2 "\""))))

;; === Font lock ===

(defconst sic-mode-font-lock-keywords
  `((,(concat "(" (regexp-opt sic-mode--keywords t) "\\_>")
     (1 font-lock-keyword-face))
    (,(concat "(" (regexp-opt sic-mode--builtins t) "\\_>")
     (1 font-lock-builtin-face))
    ;; font-lock-operator-face arrived in Emacs 29; this mode supports 28.
    (,(concat "(" (regexp-opt sic-mode--operators t) "\\_>")
     (1 (if (facep 'font-lock-operator-face)
            'font-lock-operator-face
          'font-lock-builtin-face)))
    ("(\\(#[[:alpha:]]+\\)\\_>" (1 font-lock-preprocessor-face))
    ("\\_<\\(break\\|continue\\)\\_>" (1 font-lock-keyword-face))
    ("(\\(?:fn\\|defmacro\\)\\_>[ \t\n]+\\(\\(?:\\sw\\|\\s_\\)+\\)"
     (1 font-lock-function-name-face))
    ("(\\(?:struct\\|union\\|enum\\|typedef\\)\\_>[ \t\n]+\\(\\(?:\\sw\\|\\s_\\)+\\)"
     (1 font-lock-type-face))
    ("(decl\\_>[ \t\n]+\\(\\(?:\\sw\\|\\s_\\)+\\)"
     (1 font-lock-variable-name-face))
    ("(\\(?:goto\\|label\\)\\_>[ \t\n]+\\(\\(?:\\sw\\|\\s_\\)+\\)"
     (1 font-lock-constant-face))
    ("\\_<:\\(?:\\sw\\|\\s_\\)+" . font-lock-type-face)))

;; === Indentation ===

(defcustom sic-mode-indent-rules
  '(("fn" . 3) ("defmacro" . 2) ("if" . 1) ("do" . 0) ("while" . 1)
    ("do-while" . 1) ("for" . 3) ("switch" . 1) ("case" . 1) ("default" . 0)
    ("struct" . 1) ("union" . 1) ("enum" . 1) ("launch" . 2)
    ("#define" . 1) ("#ifdef" . 1) ("#ifndef" . 1) ("#if" . 1)
    ("when" . 1) ("unless" . 1))
  "Statement heads and how many leading arguments they distinguish.
Remaining arguments are a body indented by `lisp-body-indent'.
Heads not listed here indent like function calls.  Add entries for
your own defmacro control forms."
  :type '(alist :key-type string :value-type integer))

(defun sic-mode--indent-function (indent-point state)
  "Indent (sic) forms; see `lisp-indent-function' for the contract.
Heads found in `sic-mode-indent-rules' indent as special forms with
that many distinguished arguments; everything else aligns like a
function call."
  (let ((normal-indent (current-column)))
    (goto-char (1+ (elt state 1)))
    (parse-partial-sexp (point) calculate-lisp-indent-last-sexp 0 t)
    (if (and (elt state 2)
             (not (looking-at "\\sw\\|\\s_")))
        ;; Head is itself a sexp (a computed call): align under it.
        (progn
          (unless (> (save-excursion (forward-line 1) (point))
                     calculate-lisp-indent-last-sexp)
            (goto-char calculate-lisp-indent-last-sexp)
            (beginning-of-line)
            (parse-partial-sexp (point) calculate-lisp-indent-last-sexp 0 t))
          (backward-prefix-chars)
          (current-column))
      (let* ((head (buffer-substring-no-properties
                    (point) (progn (forward-sexp 1) (point))))
             (spec (cdr (assoc head sic-mode-indent-rules))))
        (when spec
          (lisp-indent-specform spec state indent-point normal-indent))))))

(defun sic-mode--comment-indent ()
  "Indent sic comments, which use a single ; even at code level.
Lisp's convention would banish single-; comments to `comment-column';
here a comment alone on its line indents like code."
  (if (save-excursion (skip-chars-backward " \t") (bolp))
      (let ((indent (calculate-lisp-indent)))
        (if (listp indent) (car indent) indent))
    comment-column))

;; === Imenu ===

(defconst sic-mode-imenu-generic-expression
  '((nil "^(fn[ \t]+\\(\\(?:\\sw\\|\\s_\\)+\\)" 1)
    ("Macros" "^(defmacro[ \t]+\\(\\(?:\\sw\\|\\s_\\)+\\)" 1)
    ("Types" "^(\\(?:struct\\|union\\|enum\\|typedef\\)[ \t]+\\(\\(?:\\sw\\|\\s_\\)+\\)" 1)))

;; === Completion ===

(defun sic-mode--buffer-candidates ()
  "Symbols of length >= 2 from all (sic) buffers, dabbrev-style."
  (let ((seen (make-hash-table :test #'equal))
        out)
    (dolist (buf (buffer-list))
      (when (with-current-buffer buf (derived-mode-p 'sic-mode))
        (with-current-buffer buf
          (save-excursion
            (goto-char (point-min))
            (while (re-search-forward "\\_<\\(?:\\sw\\|\\s_\\)\\{2,\\}" nil t)
              (let ((sym (match-string-no-properties 0)))
                (unless (string-match-p "\\`[0-9:]" sym)
                  (puthash sym t seen))))))))
    (maphash (lambda (k _) (push k out)) seen)
    out))

(defun sic-mode--candidates ()
  (append sic-mode--keywords sic-mode--builtins '("break" "continue")
          sic-mode--preproc sic-mode--types
          (sic-mode--buffer-candidates)))

(defun sic-mode-completion-at-point ()
  "Complete form heads, :types, and symbols from (sic) buffers."
  (let ((end (point))
        (beg (save-excursion (skip-syntax-backward "w_") (point))))
    (list beg end
          (completion-table-dynamic (lambda (_) (sic-mode--candidates)) t)
          :exclusive 'no)))

;; === Flymake ===

(defcustom sic-mode-sicc-program nil
  "Path to the sicc transpiler.
When nil, look for an executable `sicc' in a dominating directory of
the buffer's file, then on `exec-path'."
  :type '(choice (const :tag "Find automatically" nil) file))

(defcustom sic-mode-cc-program "cc"
  "C compiler `sic-flymake' checks the generated C with."
  :type 'string)

(defcustom sic-mode-cc-flags '("-Wall" "-Wextra")
  "Flags passed to `sic-mode-cc-program' along with -fsyntax-only."
  :type '(repeat string))

(defcustom sic-mode-nvcc-program "nvcc"
  "CUDA compiler `sic-flymake' checks .cu.sic buffers with, if found."
  :type 'string)

(defvar-local sic-mode--flymake-proc nil)

(defun sic-mode--find-sicc ()
  (or sic-mode-sicc-program
      (when-let* ((file (or buffer-file-name default-directory))
                  (root (locate-dominating-file
                         file (lambda (dir)
                                (file-executable-p (expand-file-name "sicc" dir))))))
        (expand-file-name "sicc" root))
      (executable-find "sicc")))

(defun sic-mode--flymake-parse (source src)
  "Collect diagnostics for SOURCE from the current process buffer.
Both sicc and cc report positions in SRC's coordinates — cc because
the generated C carries #line markers pointing back at SRC — so one
pattern covers both, and lines about other files (headers) drop out."
  (goto-char (point-min))
  (let ((gcc-style (concat "^" (regexp-quote src)
                           ":\\([0-9]+\\):\\([0-9]+\\): "
                           "\\(fatal error\\|error\\|warning\\|note\\): \\(.*\\)$"))
        (nvcc-style (concat "^" (regexp-quote src)
                            "(\\([0-9]+\\)): \\(error\\|warning\\): \\(.*\\)$"))
        diags)
    (while (re-search-forward gcc-style nil t)
      (let ((region (flymake-diag-region source
                                         (string-to-number (match-string 1))
                                         (string-to-number (match-string 2))))
            (type (pcase (match-string 3)
                    ("warning" :warning)
                    ("note" :note)
                    (_ :error))))
        (push (flymake-make-diagnostic source (car region) (cdr region)
                                       type (match-string 4))
              diags)))
    (goto-char (point-min))
    (while (re-search-forward nvcc-style nil t)
      (let ((region (flymake-diag-region source
                                         (string-to-number (match-string 1))))
            (type (if (equal (match-string 2) "warning") :warning :error)))
        (push (flymake-make-diagnostic source (car region) (cdr region)
                                       type (match-string 3))
              diags)))
    (nreverse diags)))

(defun sic-flymake (report-fn &rest _args)
  "Flymake backend: sicc the buffer, then syntax-check the generated C.
For .cu.sic buffers the C stage uses nvcc when available and is
skipped otherwise, so transpile errors still surface."
  (let ((sicc (sic-mode--find-sicc)))
    (unless sicc
      (error "sic-flymake: cannot find sicc (set `sic-mode-sicc-program')"))
    (when (process-live-p sic-mode--flymake-proc)
      (kill-process sic-mode--flymake-proc))
    (let* ((source (current-buffer))
           (cuda (and buffer-file-name
                      (string-suffix-p ".cu.sic" buffer-file-name)))
           (src (make-temp-file "sic-flymake-" nil (if cuda ".cu.sic" ".sic")))
           (gen (concat src (if cuda ".cu" ".c")))
           (obj (concat src ".o"))
           (nvcc (and cuda (executable-find sic-mode-nvcc-program)))
           (check (cond
                   (nvcc (list nvcc "-c" gen "-o" obj))
                   (cuda '(":"))
                   (t `(,sic-mode-cc-program ,@sic-mode-cc-flags
                        "-fsyntax-only" ,gen))))
           (command (format "%s && %s"
                            (mapconcat #'shell-quote-argument
                                       (list sicc src gen) " ")
                            (mapconcat #'shell-quote-argument check " "))))
      (save-restriction
        (widen)
        (write-region (point-min) (point-max) src nil 'silent))
      (setq sic-mode--flymake-proc
            (make-process
             :name "sic-flymake" :noquery t :connection-type 'pipe
             :buffer (generate-new-buffer " *sic-flymake*")
             :command (list shell-file-name shell-command-switch command)
             :sentinel
             (lambda (proc _event)
               (when (memq (process-status proc) '(exit signal))
                 (unwind-protect
                     (if (with-current-buffer source
                           (eq proc sic-mode--flymake-proc))
                         (with-current-buffer (process-buffer proc)
                           (funcall report-fn
                                    (sic-mode--flymake-parse source src)))
                       (flymake-log :warning "Canceling obsolete check %s" proc))
                   (ignore-errors (delete-file src))
                   (ignore-errors (delete-file gen))
                   (ignore-errors (delete-file obj))
                   (kill-buffer (process-buffer proc))))))))))

;; === Mode ===

;;;###autoload
(define-derived-mode sic-mode lisp-data-mode "(sic)"
  "Major mode for the (sic) language: s-expressions that transpile to C.
Enable `flymake-mode' for live diagnostics, or connect eglot to
tools/sic-lsp for full clangd-backed completion and navigation."
  :syntax-table sic-mode-syntax-table
  (setq-local font-lock-defaults '(sic-mode-font-lock-keywords))
  (setq-local syntax-propertize-function sic-mode--syntax-propertize)
  (setq-local lisp-indent-function #'sic-mode--indent-function)
  (setq-local comment-indent-function #'sic-mode--comment-indent)
  (setq-local comment-add 0)
  (setq-local imenu-generic-expression sic-mode-imenu-generic-expression)
  (add-hook 'completion-at-point-functions
            #'sic-mode-completion-at-point nil t)
  (add-hook 'flymake-diagnostic-functions #'sic-flymake nil t))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.sic\\'" . sic-mode))

(provide 'sic-mode)
;;; sic-mode.el ends here
