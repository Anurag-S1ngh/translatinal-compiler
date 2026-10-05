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
trco <input_file.c> [--tokens|--ast|--ir|--translate|--emit-c] [-o <output.c>]
```

### Flags

| Flag      | Description                              |
|-----------|------------------------------------------|
| `--tokens`| Dump the token stream from lexical analysis |
| `--ast`   | Dump the Abstract Syntax Tree            |
| `--ir`    | Dump Three-Address Code IR *(default)*   |
| `--translate`, `--emit-c`, `--c` | Emit translated, re-compilable C source |
| `-o <file>` | Write translated C to `<file>` instead of stdout (requires `--translate`) |

If no flag is provided, the compiler runs the full pipeline (lex → parse → sema → IR) and emits the IR dump.

### Examples

```bash
# Compile fib.c with default IR dump
trco tests/cases/fib.c --ir

# Show tokens only
trco tests/cases/fib.c --tokens

# Show AST only
trco tests/cases/fib.c --ast

# Show all phases in sequence
trco tests/cases/fib.c --tokens --ast --ir

# Translate to clean C on stdout
trco tests/cases/fib.c --translate

# Translate to a file, then recompile with gcc
trco tests/cases/fib.c --translate -o fib.out.c
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

| Test Case   | Description                              |
|-------------|------------------------------------------|
| `fib.c`     | Recursive Fibonacci function             |
| `loops.c`   | `while`, `for`, `continue`, `break`      |
| `undeclared.c` | Expected to fail semantic analysis   |

Run `make test` to execute the full test suite.

## Output Verification

The emitted IR is correct Three-Address Code, and `--translate` emits standalone C that recompiles with any C11 compiler:

```bash
# Example IR output (abbreviated)
=== THREE-ADDRESS CODE IR ===

FUNC main:
  L0:
    %t0 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %t1)
  L1:
    ret void
```

## Translation & Compiler Correctness

`--translate` lowers the *semantically checked* AST (after type checking and
implicit-cast insertion) back to C. Correctness properties:

- **Control-flow preserved**: `if/else`, `while`, `for`, `break`, `continue`, `return` map 1:1.
- **Evaluation order preserved**: every binary op is fully parenthesized, so target-C precedence cannot change semantics.
- **Short-circuit preserved**: `&&` / `||` are emitted as-is, keeping lazy evaluation (including side-effect ordering).
- **Types preserved**: `int`/`float`/`char`/`void` signatures, declarations, and sema-inserted `(float)`/`(int)` casts are emitted explicitly.
- **Literals safe**: strings/chars are re-escaped; `for (;;)` / empty clauses round-trip; single statements are wrapped in braces to avoid dangling-`else` ambiguity.
- **Self-contained output**: `#include <stdio.h>` is prepended and nullary `()` becomes `(void)`, so `gcc -std=c11 -Wall` accepts the output cleanly.

Verify equivalence (differential testing) — the translated program must print byte-identical output to the original:

```bash
trco tests/cases/fib.c --translate -o /tmp/fib.tr.c
gcc -std=c11 -Wall /tmp/fib.tr.c -o /tmp/fib.tr
gcc -std=c11 -Wno-implicit-function-declaration tests/cases/fib.c -o /tmp/fib.orig
diff <(/tmp/fib.orig) <(/tmp/fib.tr) && echo CORRECT
```

`make test` runs this differential check automatically for `fib.c` and `loops.c`, plus recompile checks and the `undeclared.c` must-fail case.

## Troubleshooting

- **`trco: Command not found`**: Ensure you have run `make` or are in the project directory
- **Unknown flag errors**: Use `trco` without flags or with `--tokens|--ast|--ir|--translate`
- **Semantic errors**: Verify variable declarations, type consistency, and that `main()` exists
- **`-o` without `--translate`**: `-o <file>` only applies to translated-C output; add `--translate`/`--emit-c`

---

*This compiler was built as a phase‑by‑phase educational implementation targeting the classic compiler pipeline.*