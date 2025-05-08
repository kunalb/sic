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



## Source conventions / style decisions
- (Haven't written enough C yet to have taste, making things up as I go)

Names
- _init / _free

Style
- Struct / Enum
- any_function
- all_args
- 2 spaces
