# Design decisions

A running log of decisions and their rationale, so the code itself can stay
mostly comment-free. Newest entries at the bottom of each section.

## Testing

- Tests are golden files: every `.sic` file under `examples/` and
  `tests/cases/` is transpiled, compiled, and run by `tests/run.sh`; its
  stdout must match the sibling `.out` file (stdin comes from a sibling
  `.in` file if present). This exercises the full pipeline — parser,
  transpiler, and the validity of the generated C — with no test-only code
  in `sicc.c`, which is why the old `#ifdef TEST` main was removed.
- `examples/` doubles as the showcase and the integration suite;
  `tests/cases/` holds small programs that each exercise one language form.
- `tests/codegen/` holds transpile-only golden tests: the generated C
  itself (minus `#line` markers) is diffed against a checked-in `.c`
  file. For pinning emission details the run-tests can't observe, and
  for targets this machine can't compile or run (CUDA).

## Transpiler rules

- Every form is either an expression or a statement, declared on its rule.
  The first rule whose regex matches the head atom decides what the form
  *is*; context is then checked, not used for dispatch. Expressions coerce
  to statements (a `;` is appended); a statement in expression position is
  an error. This keeps dispatch predictable — a head never means two
  different things depending on where it appears — so the ternary gets its
  own head (`?:`) instead of overloading `if`.
- Relational and equality operators take exactly two operands. C parses
  `a < b < c` as `(a < b) < c`, which silently gives comparison chains the
  wrong meaning; write the individual comparisons and combine them with
  `&&` instead.

## Types

- Types are single atoms starting with `:`, and hyphens translate to
  spaces: `:const-char*` → `const char*`, `:unsigned-long` →
  `unsigned long`, `:struct-Point*` → `struct Point *`. Hyphens are
  illegal in C type names so the mapping can't collide; translation stops
  at `[` so array-size expressions like `x[N-1]` are left alone. Storage
  classes ride along for free (`:static-int`).
- Function-pointer types are a structured form rather than an atom —
  `(fnptr :ret (:argtypes...))` — because their C syntax needs
  parentheses, which the reader always splits into a sexp. The form is
  accepted anywhere a declarator is built (decl, typedef, struct fields,
  fn arguments) but not in casts or sizeof, where a plain atom is still
  required; add those when a real program needs them.
- Generated code must compile warning-free: the test harness builds with
  -Wall -Werror.

## Deliberately unsupported (so far)

- Multi-declarator statements (`int a, b;`) — write two `decl`s; most C
  style guides discourage the combined form anyway.
- String literal concatenation — use one literal with `\n` escapes.
- Anonymous and nested struct/union definitions — name the inner type
  and refer to it; revisit if tagged unions get painful in practice.
- Casts to function-pointer types and `sizeof` of one — take a
  `typedef`'d name first.

## Preprocessor

- `#ifdef` / `#ifndef` / `#if` are wrapping forms — the body lives inside
  the sexp and `#endif` is emitted automatically, so guards can't be left
  unbalanced. `(#else)` is a marker placed between the two halves of the
  body. `#define` exists in object-like and function-like shapes.
- Macro hygiene rides on expression emission: composite call-site
  arguments are already parenthesized (`(SQR (+ i 1))` → `SQR((i + 1))`),
  and operator bodies parenthesize themselves, so the classic
  unparenthesized-argument bugs mostly can't happen. Macro *parameters*
  inside bodies are not auto-wrapped, though — a body that juxtaposes a
  parameter with higher-precedence syntax by hand can still misbind.

## Macros

- `defmacro` is pure template substitution, run as a separate pass between
  parse and transpile — the emitter and rule table never see a macro form.
  No quasiquote/unquote: the single template form *is* the template, atoms
  naming parameters are replaced, everything else passes through. Bodies
  that want multiple statements wrap themselves in `(do ...)`. Procedural
  macros (bodies executed at compile time) wait for the REPL's
  compile-and-load machinery, which they'll share.
- Rest parameters spell the `...` at both declaration and use
  (`body...`), matching variadic `fn`; where the atom appears in the
  template the collected forms are spliced, not inserted as a list.
- Hygiene for introduced bindings is opt-in via auto-gensym: template
  atoms ending in `#` rename to `name__N`, shared within one expansion,
  fresh across expansions (the Clojure convention). Call-site hygiene was
  already covered by parenthesized expression emission.
- Expansion is outermost-first with a depth cap of 200, so a recursive
  macro is a positioned error instead of a hang. Expanded nodes are
  stamped with the call site's position: `#line` and later errors point
  at the user's code, never the template.
- Macros shadow builtin rules (expansion runs first, trivially) — that's
  the extensibility story, and `defmacro` is explicit enough that
  shadowing is never accidental. Redefining a *macro* name is an error,
  though. Definitions are top-level only and must precede use; scoped
  macros can come later if a real program wants them.

## CUDA

- No cuda-named machinery in the transpiler. Qualifiers (`__global__`,
  `__shared__`, `__constant__`, ...) deliberately ride on hyphen-types —
  they're just storage-class-like tokens, so `:__global__-void` works the
  way `:static-int` always has, and anything NVIDIA adds later works for
  free. Builtins (`threadIdx.x`, `dim3`, `__syncthreads`) are ordinary
  atoms and calls.
- The single form CUDA actually needs is `launch`, since C has no
  `<<<>>>` syntax: `(launch kernel (grid block [shared stream]) args...)`.
  The config sexp mirrors the chevron grouping; its elements are
  arbitrary expressions so `(dim3 16 16)` works.
- There is no C mode for `.cu` files — nvcc's frontend is C++ only, and
  "CUDA C" in practice means C-style code in the C/C++ common subset.
  sic doesn't try to enforce that (the transpiler doesn't know the
  target); the practical rules are: cast `malloc` results, keep
  designated initializers in declaration order, avoid C++ keywords as
  identifiers. The nvcc-gated test tier catches the rest.
- `*.cu.sic` is the harness's compile-with-nvcc marker; `sicc` itself
  stays toolchain-ignorant. Transpilation is always tested; compile and
  run tiers gate on nvcc and a GPU, degrading to SKIP.

## Diagnostics

- Malformed input produces `file:row:col: error: message` and exits
  immediately (`fail_at`). No error recovery or multi-error reporting: the
  transpiler is fast enough to rerun, and bailing on the first error keeps
  every rule free of recovery bookkeeping. Asserts are reserved for
  internal invariants, never user input.
- The transpiler processes exactly one file per run, so the current source
  name lives in one module-level variable (`sic_srcname`) instead of being
  threaded through every rule signature. Revisit if multi-file compilation
  ever lands.
- `#line` directives name the `.sic` source, so compiler errors from the
  generated C and debuggers both point back at the original file.
