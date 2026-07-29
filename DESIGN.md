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
