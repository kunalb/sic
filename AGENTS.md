# Working on (sic)

Guidance for coding agents (and humans). The prime directive: keep this
codebase simple enough that one person can hold all of it in their head.

## Workflow

- Make tiny, incremental commits as you go — one logical change per
  commit (one form, one bugfix, one doc pass), not a session's work
  batched into one. Write a clear message explaining the why.
- Every commit leaves `make test` green. New behavior lands with its
  golden tests in the same commit.
- Record decisions where they belong as part of the change: rationale in
  `DESIGN.md`, the language reference in `README.md`, a progress line in
  `worklog.md`.

## Style

Match the existing code in `src/sicc.c`; when in doubt, copy its idioms.

- Simplicity beats generality. The whole transpiler is one file with
  `// === Section ===` banners — keep it that way until it genuinely
  hurts. Prefer the boring implementation that is obvious on first read.
- Plain C with POSIX, no dependencies beyond libc/libm. Two-space
  indentation; typedef'd structs and forward declarations at the top.
- `_init`/`_free` pairs for anything heap-allocated; every allocation
  goes through `CHECK_ALLOC`. Growable buffers follow the
  `len`/`buffer_len` doubling pattern of `List` and `Atom` — don't
  invent a new container idiom.
- Prefer a small data table with uniform dispatch (like
  `TRANSPILE_RULES`) over scattered special cases.
- Errors in user input go through `fail_at` with a position and die
  immediately — no recovery. Messages teach correct usage:
  `"decl needs a name and a :type, e.g. (decl x :int)"`. Asserts are
  for internal invariants only, never user input.
- Comments state only constraints the code can't show. The why lives in
  `DESIGN.md`, not in comments.
- Tests are golden files — no test-only code in `sicc.c`. One small
  program per language form in `tests/cases/`, expected diagnostics in
  `tests/errors/`, real programs in `examples/`. Generated C must
  compile with `-Wall -Werror`.
