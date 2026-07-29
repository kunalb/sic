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
