#ifndef TRCO_CODEGEN_H
#define TRCO_CODEGEN_H

#include "ast.h"
#include <stdio.h>

// Translation backend: lowers a semantically-checked AST to clean,
// recompilable C (source-to-source translation).
//
// Correctness note: the emitter preserves the exact evaluation order
// and control-flow structure of the source program. Every binary
// operation is fully parenthesized so C precedence cannot change
// semantics; short-circuit operators (&&, ||) are emitted as-is so
// the target compiler preserves lazy evaluation.

void codegen_emit_program(ASTProgram *prog, FILE *out);
int codegen_emit_to_file(ASTProgram *prog, const char *path);

#endif // TRCO_CODEGEN_H
