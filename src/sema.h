#ifndef TRCO_SEMA_H
#define TRCO_SEMA_H

#include "ast.h"
#include <stdbool.h>

// Symbol entry
typedef struct Symbol {
    char *name;
    DataType type;
    bool is_function;
    // Function specific
    DataType return_type;
    DataType *param_types;
    int param_count;
    struct Symbol *next;
} Symbol;

// Scope
typedef struct Scope {
    Symbol *symbols;
    struct Scope *parent;
} Scope;

typedef struct {
    Scope *current_scope;
    ASTFunction *current_function;
    int loop_depth;
} SemaAnalyzer;

bool sema_analyze_program(ASTProgram *prog);

#endif // TRCO_SEMA_H
