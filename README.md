# The (sic) Programming Language
## Symbolic-Expressions in C; *intentionally so*

`(sic)` aims to be a minimal, extremely extensible programming language -- that transpiles to modern C -- aimed at solo hackers or small teams for rapid prototyping and quickly building reusable applications.

## Design principles
- Minimal core footprint; with all layers implemented within the language.
- Extreme meta-programming and macro support.
- Support for interactive use built in from day 1.
- Trivial to profile and debug, with abstractions easily demonstrating their cost.
- Works with all modern tools: LSPs, ebpf, gdb, LLMs, etc.
- Programming should be fun.


## Folder structure
- src: language core and implementation
- batteries: runtime libraries, released and versioned separately
- examples: different projects built in sicc to test out the language


## Structure, conventions
- (Haven't written enough C yet to have taste, making things up as I go)

Structure
- First pass: convert symbolic expressions to list of sexps/attoms
- Second pass: transpiler applies rules to generate code

Conventions
- 2 space indentation
- _init / _free for constructor/destructor


## Plan

Get the basics working
- Transpilation: rule support; do it in a way that is pluggable
- Clean up data structures for lists, atoms, and s-expressions
  - S-expression annotations / second order details
- Error propagation; introduce Result objects
- Compiler tools: wrap calls to gcc/clang to make it transparent/interactive
- Document syntactic choices and special expressions

Write code with sexpressions while filling in the transpiler rules / exploring C
- Advent of Code 2024 basics
- Translate llm.c
- Make sure cuda kernels can also be transpiled

Add REPL support
- Figure out a meaningful repl/ways to do this interactively
- Add custom dlopen/dlsym handling etc for ergonomic support

Iterate on language design and build tools for convenience
- LSP support, autocomplete; wrap clang-based tools
- Tree-Sitter/syntax highlighting support (shoud be trivial)
- Second order tools that can be built within the language (automatic annotation, etc.)
- Compilation / packaging support

Extensibility/macro support
- Explore additional compile time execution to expand macros/do reflection
- Dig into transpiler performance and bottlenecks
- Allow registering rules within the language itself

Typing & safety exploration
- Potentially support generics, additional macros for types
- Revisit options for reflection support

Batteries
- Convenient runtime support for dictionaries, tries, other basic data structures
- Wrap or build from scratch in a way that just works

More extensions
- Extensions / sugar like `defer` support and other conveniences offered by modern C

Announce/share
- Build a website, clean up official documentation; tutorials
- Select a reasonable forum for discussion
