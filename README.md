# trco — A C-to-C Translational Compiler

## Overview

`trco` is a source-to-source compiler that translates a practical subset of C into clean, re-compilable C. It passes through the classic compiler phases:

1. **Lexical Analysis** — Tokenizes the input source code
2. **Syntax Analysis** — Parses tokens into an Abstract Syntax Tree (AST)
3. **Semantic Analysis** — Performs type checking, scope resolution, and error detection
4. **Intermediate Representation (IR) Generation** — Lowers the AST to Three-Address Code (TAC) quads

## Building

```bash
make             # Compiles the trco binary
make test        # Compiles and runs the test suite
make clean       # Removes object files and binary
```

## Usage

```bash
trco <input_file.c> [--tokens|--ast|--ir]
```

### Flags

| Flag      | Description                              |
|-----------|------------------------------------------|
| `--tokens`| Dump the token stream from lexical analysis |
| `--ast`   | Dump the Abstract Syntax Tree            |
| `--ir`    | Dump Three-Address Code IR *(default)*   |

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

The emitted IR is designed to be correct Three-Address Code; however, it is **not** yet compiled into a standalone executable. The compiler's purpose is to demonstrate the phases (lexical → syntax → semantic → IR) and provide clean dumps for inspection.

```bash
# Example IR output (abbreviated)
=== THREE-ADDRESS CODE IR ===

FUNC main:
  L0:
    %t0 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %t1)
  L1:
    ret void
```

## Troubleshooting

- **`trco: Command not found`**: Ensure you have run `make` or are in the project directory
- **Unknown flag errors**: Use `trco` without flags or with `--tokens|--ast|--ir`
- **Semantic errors**: Verify variable declarations, type consistency, and that `main()` exists

---

*This compiler was built as a phase‑by‑phase educational implementation targeting the classic compiler pipeline.*