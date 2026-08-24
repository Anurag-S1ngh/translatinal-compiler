#include "sema.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Scope *scope_push(Scope *parent) {
    Scope *s = xmalloc(sizeof(Scope));
    s->symbols = NULL;
    s->parent = parent;
    return s;
}

static Scope *scope_pop(Scope *current) {
    if (!current) return NULL;
    Scope *parent = current->parent;
    Symbol *sym = current->symbols;
    while (sym) {
        Symbol *next = sym->next;
        free(sym->name);
        if (sym->is_function && sym->param_types) {
            free(sym->param_types);
        }
        free(sym);
        sym = next;
    }
    free(current);
    return parent;
}

static Symbol *symbol_lookup_current(Scope *scope, const char *name) {
    if (!scope) return NULL;
    for (Symbol *s = scope->symbols; s; s = s->next) {
        if (strcmp(s->name, name) == 0) return s;
    }
    return NULL;
}

static Symbol *symbol_lookup(Scope *scope, const char *name) {
    for (Scope *s = scope; s; s = s->parent) {
        Symbol *sym = symbol_lookup_current(s, name);
        if (sym) return sym;
    }
    return NULL;
}

static Symbol *symbol_add_var(Scope *scope, const char *name, DataType type, int line, int col) {
    if (symbol_lookup_current(scope, name)) {
        error_at(line, col, "Redeclaration of variable '%s' in same scope", name);
        return NULL;
    }
    Symbol *sym = xmalloc(sizeof(Symbol));
    sym->name = xstrdup(name);
    sym->type = type;
    sym->is_function = false;
    sym->param_types = NULL;
    sym->param_count = 0;
    sym->next = scope->symbols;
    scope->symbols = sym;
    return sym;
}

static Symbol *symbol_add_func(Scope *scope, const char *name, DataType ret_type, DataType *param_types, int param_count, int line, int col) {
    if (symbol_lookup_current(scope, name)) {
        error_at(line, col, "Redeclaration of function '%s'", name);
        return NULL;
    }
    Symbol *sym = xmalloc(sizeof(Symbol));
    sym->name = xstrdup(name);
    sym->type = ret_type;
    sym->is_function = true;
    sym->return_type = ret_type;
    sym->param_count = param_count;
    if (param_count > 0) {
        sym->param_types = xmalloc(sizeof(DataType) * param_count);
        memcpy(sym->param_types, param_types, sizeof(DataType) * param_count);
    } else {
        sym->param_types = NULL;
    }
    sym->next = scope->symbols;
    scope->symbols = sym;
    return sym;
}

// Forward decls
static void sema_expr(SemaAnalyzer *sa, ASTExpr **expr_ptr);
static void sema_stmt(SemaAnalyzer *sa, ASTStmt *stmt);

static ASTExpr *insert_cast(ASTExpr *expr, DataType target_type) {
    if (expr->eval_type == target_type) return expr;
    return ast_expr_cast(target_type, expr, expr->line, expr->col);
}

static void sema_expr(SemaAnalyzer *sa, ASTExpr **expr_ptr) {
    if (!expr_ptr || !*expr_ptr) return;
    ASTExpr *e = *expr_ptr;

    switch (e->kind) {
        case EXPR_INT_LIT:
            e->eval_type = TYPE_INT;
            break;
        case EXPR_FLOAT_LIT:
            e->eval_type = TYPE_FLOAT;
            break;
        case EXPR_CHAR_LIT:
            e->eval_type = TYPE_CHAR;
            break;
        case EXPR_STR_LIT:
            e->eval_type = TYPE_UNKNOWN; // special string literal
            break;
        case EXPR_VAR: {
            Symbol *sym = symbol_lookup(sa->current_scope, e->var_name);
            if (!sym) {
                error_at(e->line, e->col, "Use of undeclared variable '%s'", e->var_name);
                e->eval_type = TYPE_INT; // Fallback
            } else if (sym->is_function) {
                error_at(e->line, e->col, "'%s' is a function, not a variable", e->var_name);
                e->eval_type = TYPE_INT;
            } else {
                e->eval_type = sym->type;
            }
            break;
        }
        case EXPR_UNARY: {
            sema_expr(sa, &e->unary.operand);
            if (e->unary.op == UNARY_NOT) {
                e->eval_type = TYPE_INT;
            } else {
                e->eval_type = e->unary.operand->eval_type;
            }
            break;
        }
        case EXPR_BINARY: {
            sema_expr(sa, &e->binary.left);
            sema_expr(sa, &e->binary.right);

            DataType lt = e->binary.left->eval_type;
            DataType rt = e->binary.right->eval_type;

            // Relational & Logical output is int (boolean 0 or 1)
            if (e->binary.op >= BIN_EQ && e->binary.op <= BIN_OR) {
                e->eval_type = TYPE_INT;
                // Coerce int and float if needed
                if (lt == TYPE_FLOAT && rt == TYPE_INT) {
                    e->binary.right = insert_cast(e->binary.right, TYPE_FLOAT);
                } else if (lt == TYPE_INT && rt == TYPE_FLOAT) {
                    e->binary.left = insert_cast(e->binary.left, TYPE_FLOAT);
                }
            } else {
                // Arithmetic (+, -, *, /, %)
                if (lt == TYPE_FLOAT || rt == TYPE_FLOAT) {
                    e->eval_type = TYPE_FLOAT;
                    if (lt == TYPE_INT || lt == TYPE_CHAR) {
                        e->binary.left = insert_cast(e->binary.left, TYPE_FLOAT);
                    }
                    if (rt == TYPE_INT || rt == TYPE_CHAR) {
                        e->binary.right = insert_cast(e->binary.right, TYPE_FLOAT);
                    }
                } else {
                    e->eval_type = TYPE_INT;
                }
            }
            break;
        }
        case EXPR_ASSIGN: {
            Symbol *sym = symbol_lookup(sa->current_scope, e->assign.var_name);
            if (!sym) {
                error_at(e->line, e->col, "Assignment to undeclared variable '%s'", e->assign.var_name);
            } else if (sym->is_function) {
                error_at(e->line, e->col, "Cannot assign to function '%s'", e->assign.var_name);
            }
            sema_expr(sa, &e->assign.value);
            if (sym) {
                e->eval_type = sym->type;
                if (sym->type == TYPE_FLOAT && e->assign.value->eval_type == TYPE_INT) {
                    e->assign.value = insert_cast(e->assign.value, TYPE_FLOAT);
                } else if (sym->type == TYPE_INT && e->assign.value->eval_type == TYPE_FLOAT) {
                    e->assign.value = insert_cast(e->assign.value, TYPE_INT);
                }
            } else {
                e->eval_type = e->assign.value->eval_type;
            }
            break;
        }
        case EXPR_CALL: {
            Symbol *sym = symbol_lookup(sa->current_scope, e->call.func_name);
            // Built-in functions check (e.g., printf, putchar)
            if (!sym && (strcmp(e->call.func_name, "printf") == 0 || strcmp(e->call.func_name, "putchar") == 0)) {
                // Allow variadic / builtins
                for (int i = 0; i < e->call.arg_count; i++) {
                    sema_expr(sa, &e->call.args[i]);
                }
                e->eval_type = TYPE_INT;
                break;
            }

            if (!sym) {
                error_at(e->line, e->col, "Call to undeclared function '%s'", e->call.func_name);
                e->eval_type = TYPE_INT;
            } else if (!sym->is_function) {
                error_at(e->line, e->col, "'%s' is a variable, not a function", e->call.func_name);
                e->eval_type = TYPE_INT;
            } else {
                e->eval_type = sym->return_type;
                if (e->call.arg_count != sym->param_count) {
                    error_at(e->line, e->col, "Function '%s' expects %d arguments, but got %d",
                             e->call.func_name, sym->param_count, e->call.arg_count);
                }
                for (int i = 0; i < e->call.arg_count; i++) {
                    sema_expr(sa, &e->call.args[i]);
                    if (i < sym->param_count) {
                        DataType expected = sym->param_types[i];
                        if (expected == TYPE_FLOAT && e->call.args[i]->eval_type == TYPE_INT) {
                            e->call.args[i] = insert_cast(e->call.args[i], TYPE_FLOAT);
                        } else if (expected == TYPE_INT && e->call.args[i]->eval_type == TYPE_FLOAT) {
                            e->call.args[i] = insert_cast(e->call.args[i], TYPE_INT);
                        }
                    }
                }
            }
            break;
        }
        case EXPR_CAST:
            sema_expr(sa, &e->cast.expr);
            e->eval_type = e->cast.target_type;
            break;
    }
}

static void sema_stmt(SemaAnalyzer *sa, ASTStmt *stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case STMT_DECL: {
            if (stmt->decl.type == TYPE_VOID) {
                error_at(stmt->line, stmt->col, "Cannot declare variable '%s' of type void", stmt->decl.var_name);
            }
            if (stmt->decl.init) {
                sema_expr(sa, &stmt->decl.init);
                if (stmt->decl.type == TYPE_FLOAT && stmt->decl.init->eval_type == TYPE_INT) {
                    stmt->decl.init = insert_cast(stmt->decl.init, TYPE_FLOAT);
                } else if (stmt->decl.type == TYPE_INT && stmt->decl.init->eval_type == TYPE_FLOAT) {
                    stmt->decl.init = insert_cast(stmt->decl.init, TYPE_INT);
                }
            }
            symbol_add_var(sa->current_scope, stmt->decl.var_name, stmt->decl.type, stmt->line, stmt->col);
            break;
        }
        case STMT_EXPR:
            sema_expr(sa, &stmt->expr);
            break;
        case STMT_BLOCK: {
            sa->current_scope = scope_push(sa->current_scope);
            for (int i = 0; i < stmt->block.stmt_count; i++) {
                sema_stmt(sa, stmt->block.stmts[i]);
            }
            sa->current_scope = scope_pop(sa->current_scope);
            break;
        }
        case STMT_IF:
            sema_expr(sa, &stmt->if_stmt.cond);
            sema_stmt(sa, stmt->if_stmt.then_branch);
            if (stmt->if_stmt.else_branch) {
                sema_stmt(sa, stmt->if_stmt.else_branch);
            }
            break;
        case STMT_WHILE:
            sema_expr(sa, &stmt->while_stmt.cond);
            sa->loop_depth++;
            sema_stmt(sa, stmt->while_stmt.body);
            sa->loop_depth--;
            break;
        case STMT_FOR:
            sa->current_scope = scope_push(sa->current_scope);
            if (stmt->for_stmt.init) {
                sema_stmt(sa, stmt->for_stmt.init);
            }
            if (stmt->for_stmt.cond) {
                sema_expr(sa, &stmt->for_stmt.cond);
            }
            if (stmt->for_stmt.step) {
                sema_expr(sa, &stmt->for_stmt.step);
            }
            sa->loop_depth++;
            sema_stmt(sa, stmt->for_stmt.body);
            sa->loop_depth--;
            sa->current_scope = scope_pop(sa->current_scope);
            break;
        case STMT_RETURN:
            if (stmt->return_stmt.value) {
                if (sa->current_function && sa->current_function->return_type == TYPE_VOID) {
                    error_at(stmt->line, stmt->col, "Void function '%s' should not return a value", sa->current_function->name);
                }
                sema_expr(sa, &stmt->return_stmt.value);
                if (sa->current_function) {
                    DataType ret = sa->current_function->return_type;
                    if (ret == TYPE_FLOAT && stmt->return_stmt.value->eval_type == TYPE_INT) {
                        stmt->return_stmt.value = insert_cast(stmt->return_stmt.value, TYPE_FLOAT);
                    } else if (ret == TYPE_INT && stmt->return_stmt.value->eval_type == TYPE_FLOAT) {
                        stmt->return_stmt.value = insert_cast(stmt->return_stmt.value, TYPE_INT);
                    }
                }
            } else {
                if (sa->current_function && sa->current_function->return_type != TYPE_VOID) {
                    error_at(stmt->line, stmt->col, "Non-void function '%s' must return a value", sa->current_function->name);
                }
            }
            break;
        case STMT_BREAK:
            if (sa->loop_depth <= 0) {
                error_at(stmt->line, stmt->col, "'break' statement not within a loop");
            }
            break;
        case STMT_CONTINUE:
            if (sa->loop_depth <= 0) {
                error_at(stmt->line, stmt->col, "'continue' statement not within a loop");
            }
            break;
    }
}

bool sema_analyze_program(ASTProgram *prog) {
    SemaAnalyzer sa;
    sa.current_scope = scope_push(NULL); // Global scope
    sa.current_function = NULL;
    sa.loop_depth = 0;

    // First pass: register all function signatures in global scope
    for (int i = 0; i < prog->function_count; i++) {
        ASTFunction *fn = prog->functions[i];
        DataType *ptypes = xmalloc(sizeof(DataType) * (fn->param_count > 0 ? fn->param_count : 1));
        for (int p = 0; p < fn->param_count; p++) {
            ptypes[p] = fn->params[p].type;
        }
        symbol_add_func(sa.current_scope, fn->name, fn->return_type, ptypes, fn->param_count, fn->line, fn->col);
        free(ptypes);
    }

    // Second pass: analyze function bodies
    for (int i = 0; i < prog->function_count; i++) {
        ASTFunction *fn = prog->functions[i];
        sa.current_function = fn;

        // Function body scope (parameters live here)
        sa.current_scope = scope_push(sa.current_scope);
        for (int p = 0; p < fn->param_count; p++) {
            symbol_add_var(sa.current_scope, fn->params[p].name, fn->params[p].type, fn->params[p].line, fn->col);
        }

        // Analyze function body statement block
        if (fn->body && fn->body->kind == STMT_BLOCK) {
            for (int s = 0; s < fn->body->block.stmt_count; s++) {
                sema_stmt(&sa, fn->body->block.stmts[s]);
            }
        } else {
            sema_stmt(&sa, fn->body);
        }

        sa.current_scope = scope_pop(sa.current_scope);
    }

    // Check if main function exists
    Symbol *main_sym = symbol_lookup(sa.current_scope, "main");
    if (!main_sym || !main_sym->is_function) {
        error_at(1, 1, "Program lacks a 'main' function");
    }

    // Clean up global scope
    sa.current_scope = scope_pop(sa.current_scope);

    return !g_had_error;
}
