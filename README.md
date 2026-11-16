# trco — A C-to-C Translational Compiler

## Overview

`trco` is a source-to-source compiler that translates a practical subset of C into clean, re-compilable C. It passes through the classic compiler phases:

1. **Lexical Analysis** — Tokenizes the input source code
2. **Syntax Analysis** — Parses tokens into an Abstract Syntax Tree (AST)
3. **Semantic Analysis** — Performs type checking, scope resolution, and error detection
4. **Intermediate Representation (IR) Generation** — Lowers the AST to Three-Address Code (TAC) quads
5. **C Translation (Code Generation)** — Emits clean, re-compilable C from the checked AST

## Building

```bash
make             # Compiles the trco binary
make test        # Compiles and runs the test suite
make clean       # Removes object files and binary
```

## Usage

```bash
# Simplest form — translate a.c into re-compilable o.c:
trco --input a.c --output o.c

# Full form:
trco [--input <source.c|-> | <source.c|->] [--tokens|--ast|--ir|--translate|--emit-c|--c] [--output|-o <target.c>]
```

trco takes a **source file** as input and writes either a phase dump or a **target file** of translated, re-compilable C. Use `-` as the input file to read the source from standard input.

| Flag      | Description                              |
|-----------|------------------------------------------|
| `--input <file>` | Source C file to read (`-` reads stdin). May also be given as the bare argument |
| `--output <file>`, `-o <file>` | Write the translated, re-compilable C to `<file>` (implies `--translate`) |
| `--tokens`| Dump the token stream from lexical analysis |
| `--ast`   | Dump the Abstract Syntax Tree            |
| `--ir`    | Dump Three-Address Code IR *(default)*   |
| `--translate`, `--emit-c`, `--c` | Emit translated, re-compilable C source |

If no flag is provided, the compiler runs the full pipeline (lex → parse → sema → IR) and emits the IR dump.

### Examples

```bash
# Compile fib.c with default IR dump
trco tests/cases/fib.c --ir

# Read the source from stdin
trco - --ir < tests/cases/fib.c

# Show tokens only
trco tests/cases/fib.c --tokens

# Show AST only
trco tests/cases/fib.c --ast

# Show all phases in sequence
trco tests/cases/fib.c --tokens --ast --ir

# Translate to clean C on stdout
trco tests/cases/fib.c --translate

# Translate source.c to a target file, then recompile with gcc
trco --input tests/cases/fib.c --output fib.out.c
gcc -std=c11 -Wall fib.out.c -o fib.out
./fib.out
```

## Supported C Subset

trco handles a practical subset of C including:

- **Data types**: `int`, `float`, `char`, `void`
- **Variables**: Declarations with optional initializers
- **Control flow**: `if / else`, `while`, `for`, `break`, `continue`, `return`
- **Operators**: Arithmetic (`+`, `-`, `*`, `/`, `%`), comparison (`==`, `!=`, `<`, `<=`, `>`, `>=`), logical (`&&`, `||`, `!`)
- **Function calls**: User-defined functions with parameters
- **Expressions**: Precedence-aware with parenthesization support
- **Standard library**: `printf()` calls supported

## Test Cases

The project includes test programs under `tests/cases/`:

| Test Case     | Description                                      |
|---------------|--------------------------------------------------|
| `fib.c`       | Recursive Fibonacci function                     |
| `loops.c`     | `while`, `for`, `continue`, `break`              |
| `floats.c`    | `float` arithmetic, `char` variables, mixed `int`/`float` expressions |
| `nested.c`    | Nested `for`/`while` loops with `break`/`continue` |
| `include.c`   | `#include` directive — directives are skipped      |
| `undeclared.c`| Expected to fail semantic analysis               |

Run `make test` to execute the full test suite.

## Output Verification

The emitted IR is correct Three-Address Code, and `--translate` emits standalone C that recompiles with any C11 compiler:

```bash
# Example IR output (abbreviated)
=== THREE-ADDRESS CODE IR ===

FUNC fib:
  %t0 = n <= 1
  ifz %t0 goto .L0
  return n
  goto .L1
.L0:
.L1:
  %t1 = n - 1
  param %t1
  %t2 = call fib, 1
  ...
END FUNC fib
```

## Translation & Compiler Correctness

`--translate` lowers the *semantically checked* AST (after type checking and
implicit-cast insertion) back to C. Correctness properties:

- **Control-flow preserved**: `if/else`, `while`, `for`, `break`, `continue`, `return` map 1:1.
- **Evaluation order preserved**: every binary op is fully parenthesized, so target-C precedence cannot change semantics.
- **Short-circuit preserved**: `&&` / `||` are emitted as-is, keeping lazy evaluation (including side-effect ordering).
- **Types preserved**: `int`/`float`/`char`/`void` signatures, declarations, and sema-inserted `(float)`/`(int)` casts are emitted explicitly.
- **Literals safe**: strings/chars are re-escaped; `for (;;)` / empty clauses round-trip; single statements are wrapped in braces to avoid dangling-`else` ambiguity.
- **Self-contained output**: `#include <stdio.h>` is prepended and nullary `()` becomes `(void)`, so `gcc`/`clang` with `-std=c11 -Wall -Wextra` accept the output cleanly.

Verify equivalence (differential testing) — the translated program must print byte-identical output to the original:

```bash
trco tests/cases/fib.c --translate -o /tmp/fib.tr.c
gcc -std=c11 -Wall /tmp/fib.tr.c -o /tmp/fib.tr
gcc -std=c11 -Wno-implicit-function-declaration tests/cases/fib.c -o /tmp/fib.orig
diff <(/tmp/fib.orig) <(/tmp/fib.tr) && echo CORRECT
```

`make test` runs this differential check automatically for `fib.c`, `loops.c`, `floats.c` and `nested.c`, plus recompile checks, the `-o` target-file path, stdin input, the `--emit-c`/`--c` aliases, and the `undeclared.c` must-fail case. The translated output compiles warning-free under `gcc -std=c11 -Wall -Wextra` and `clang -std=c11 -Wall -Wextra`.

The project also ships a GitHub Actions workflow (`.github/workflows/ci.yml`) that builds `trco`, runs the test suite, and recompiles every translation on each push and pull request.

## Limitations

trco deliberately implements a **practical subset** of C, not the full language. Unsupported features include:

- Pointers, arrays, and the `&`/`*` operators
- `struct`, `union`, `enum`, `typedef`
- Global/static variables — top-level code must be functions
- **The preprocessor** (`#include`, `#define`, ...) is not implemented — lines starting with `#` are **skipped**, never expanded. `#include` headers are ignored (translated output emits the `#include <stdio.h>` it needs on its own); macros are **not** expanded, so `#define`d names will surface as undeclared-variable errors. `#include`-less test programs call `printf` relying on the implicit-declaration convention
- The ternary operator, `switch`, `do/while`, `goto`, `sizeof`
- Compound assignment operators (`+=`, `-=`, ...)
- Forward function declarations/prototypes — functions must be defined before use in the order accepted by the parser

Type checking covers `int`, `float`, `char` and `void`, with implicit `int`↔`float` conversions inserted during semantic analysis.

## Troubleshooting

- **`trco: Command not found`**: Ensure you have run `make` or are in the project directory
- **Unknown flag errors**: Use `trco` without flags or with `--tokens|--ast|--ir|--translate`
- **Semantic errors**: Verify variable declarations, type consistency, and that `main()` exists
- **`-o` / `--output` writes translated C directly**: It implies `--translate`, so `trco a.c -o o.c` works without any other flag

---

*This compiler was built as a phase‑by‑phase educational implementation targeting the classic compiler pipeline.*