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

## Transpiler rules

- Every form is either an expression or a statement, declared on its rule.
  The first rule whose regex matches the head atom decides what the form
  *is*; context is then checked, not used for dispatch. Expressions coerce
  to statements (a `;` is appended); a statement in expression position is
  an error. This keeps dispatch predictable — a head never means two
  different things depending on where it appears — so the ternary gets its
  own head (`?:`) instead of overloading `if`.

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
