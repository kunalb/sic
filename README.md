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


## Getting started

```sh
make                                  # builds ./sicc
make test                             # golden tests over examples/ and tests/
./sicc examples/hello.sic hello.c && cc -o hello hello.c && ./hello
```

`sicc` writes the generated C to stdout when no output file is given.
Design decisions and their rationale live in `DESIGN.md`.

## Language reference

A program is a sequence of forms. Atoms — numbers, strings, characters,
identifiers — pass straight through as C tokens (so `a[i]` and `NULL` work
as-is), and a form with an unrecognized head is a function call:
`(printf "%d\n" x)`. A `;` starts a comment that runs to end of line.

**Types** are atoms starting with `:`, with hyphens standing in for
spaces: `:int`, `:char**`, `:int[4]`, `:const-char*`, `:unsigned-long`,
`:struct-Point*`. A type applied to a value is a cast: `(:double x)`.

**Declarations and assignment**

```lisp
(decl x :int)                 ; int x;
(decl x :int 42)              ; int x = 42;
(set place value)             ; place = value; place is any lvalue:
(set (aref a i) 0)            ;   a[i] = 0;
(set (-> node next) NULL)     ;   node->next = NULL;
(+= x 1)                      ; also -= *= /= %= &= |= ^= <<= >>=
(++ x) (-- x)
```

**Initializers**: `(init ...)` is a brace initializer; elements may be
`(.field value)` designators, and nesting works for matrices and structs
of structs. A cast around one makes a compound literal:

```lisp
(decl a :int[3] (init 1 2 3))
(decl q :struct-Point (init (.y 9)))
(decl m :int[2][2] (init (init 1 2) (init 3 4)))
(set p (:struct-Point (init 40 2)))
```

**Operators** are parenthesized. Arithmetic, logical, bitwise, and shift
operators are n-ary: `+ - * / % && || & | ^ << >>`, e.g. `(+ a b c)` is
`(a + b + c)`. Comparisons `< > <= >= == !=` take exactly two operands;
combine comparisons explicitly with `&&`. Two-element forms of
`+ - * & ! ~` are prefix operators, so `(- x)` negates, `(& x)` takes an
address, and `(* p)` dereferences (`(deref p)` also works). The ternary is
`(?: cond a b)`; the comma operator is `(, a b)`. `sizeof` takes a value
or a type — `(sizeof x)`, `(sizeof :unsigned-char)` — and
`(offsetof :struct-Pair b)` / `(alignof :double)` take types.

**Access**: `(aref a i j)` indexes (chainable), `(-> p field)` and
`(. s field)` reach into structs and also chain: `(-> a b c)` is
`a->b->c`.

**Control flow**

```lisp
(if cond then-stmt else-stmt)    ; else optional; (do ...) groups statements
(while cond stmt...)
(do-while cond stmt...)
(for (decl i :int 0) (< i n) (++ i) stmt...)
(switch v (case 1 stmt... break) (default stmt...))  ; C fallthrough, explicit break
(goto out) (label out)
(return x) (return)              ; break and continue are bare atoms
```

**Functions and records**

```lisp
(fn main :int (argc :int argv :char**) stmt...)
(fn is_odd :int (n :int))        ; no body: a prototype / forward declaration
(fn sum :int (n :int ...) ...)   ; variadic; use va_list/va_start/va_arg as calls
(decl errno_copy :extern-int)    ; storage classes ride along in the type
(struct Point x :int y :int)
(struct Flags ready :unsigned:1 mode :unsigned:3)   ; bitfields
(union Word i :int c :char[4])
(enum Color RED GREEN (BLUE 5))
(typedef Point :struct-Point)
(#include <stdio.h> "mylib.h")
```

**Preprocessor**

```lisp
(#define MAX 3)
(#define (SQR x) (* x x))          ; function-like macro
(#undef MAX)
(#ifdef VERBOSE stmt... (#else) stmt...)   ; #endif is implicit
(#ifndef GUARD stmt...)
(#if (>= MAX 3) stmt...)
(#pragma omp parallel for)
```

**Macros** are compile-time templates: `(defmacro name (params) template)`
substitutes the call-site sexps into the template — no evaluation, pure
tree rewriting — before transpilation. A final parameter ending in `...`
collects the remaining forms and splices them where it appears in the
template. Atoms ending in `#` (like `tmp#`) rename to a fresh identifier
per expansion, so introduced locals can't capture the caller's variables.
Macros must be defined before use, at the top level, and may use other
macros in their templates; a macro may even redefine a builtin form.

```lisp
(defmacro when (cond body...)
  (if cond (do body...)))

(defmacro swap (a b)
  (do (decl tmp# :int a)
      (set a b)
      (set b tmp#)))

(when (< x 10)
  (printf "small\n")
  (+= x 1))
; expands to: (if (< x 10) (do (printf "small\n") (+= x 1)))
```

**Function pointers** are the type form `(fnptr :ret (:argtypes...))`,
usable in `decl`, `typedef`, struct fields, and `fn` arguments:

```lisp
(decl cmp (fnptr :int (:const-void* :const-void*)) my_compare)
(typedef BinOp (fnptr :int (:int :int)))
```

A call whose head is an expression calls through it: `((. op apply) 4 5)`
is `((op).apply)(4, 5)`.

**CUDA** needs almost nothing special: qualifiers ride on hyphen-types
(`:__global__-void`, `:__shared__-float[256]`, `:__device__-float`),
builtins like `threadIdx.x` and `__syncthreads` pass through as atoms
and calls, and the one piece of syntax C lacks is the kernel launch:

```lisp
(fn saxpy :__global__-void (n :int a :float x :float* y :float*)
  (decl i :int (+ (* blockIdx.x blockDim.x) threadIdx.x))
  (if (< i n) (set (aref y i) (+ (* a (aref x i)) (aref y i)))))

(launch saxpy (blocks threads) n 2.0f d_x d_y)
; → saxpy<<<blocks, threads>>>(n, 2.0f, d_x, d_y);
(launch k ((dim3 16 16) (dim3 8 8) smem-bytes stream) args)
```

Name the file `*.cu.sic` so the test harness compiles it with nvcc
(skipping cleanly when nvcc or a GPU is absent). nvcc compiles `.cu` as
C++, so cast `malloc` results — `(:float* (malloc bytes))` — and keep
designated initializers in declaration order. See `examples/saxpy.cu.sic`.

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
