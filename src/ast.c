#include "ast.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *data_type_to_string(DataType type) {
    switch (type) {
        case TYPE_VOID: return "void";
        case TYPE_INT: return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_CHAR: return "char";
        case TYPE_UNKNOWN: return "unknown";
    }
    return "unknown";
}

const char *unary_op_to_string(UnaryOp op) {
    switch (op) {
        case UNARY_NEG: return "-";
        case UNARY_NOT: return "!";
        case UNARY_PRE_INC: return "++ (pre)";
        case UNARY_PRE_DEC: return "-- (pre)";
        case UNARY_POST_INC: return "++ (post)";
        case UNARY_POST_DEC: return "-- (post)";
    }
    return "?";
}

const char *binary_op_to_string(BinaryOp op) {
    switch (op) {
        case BIN_ADD: return "+";
        case BIN_SUB: return "-";
        case BIN_MUL: return "*";
        case BIN_DIV: return "/";
        case BIN_MOD: return "%";
        case BIN_EQ: return "==";
        case BIN_NEQ: return "!=";
        case BIN_LT: return "<";
        case BIN_LTE: return "<=";
        case BIN_GT: return ">";
        case BIN_GTE: return ">=";
        case BIN_AND: return "&&";
        case BIN_OR: return "||";
    }
    return "?";
}

ASTExpr *ast_expr_int_lit(long val, int line, int col) {
    ASTExpr *e = xmalloc(sizeof(ASTExpr));
    e->kind = EXPR_INT_LIT;
    e->eval_type = TYPE_INT;
    e->line = line;
    e->col = col;
    e->int_val = val;
    return e;
}

ASTExpr *ast_expr_float_lit(double val, int line, int col) {
    ASTExpr *e = xmalloc(sizeof(ASTExpr));
    e->kind = EXPR_FLOAT_LIT;
    e->eval_type = TYPE_FLOAT;
    e->line = line;
    e->col = col;
    e->float_val = val;
    return e;
}

ASTExpr *ast_expr_char_lit(char val, int line, int col) {
    ASTExpr *e = xmalloc(sizeof(ASTExpr));
    e->kind = EXPR_CHAR_LIT;
    e->eval_type = TYPE_CHAR;
    e->line = line;
    e->col = col;
    e->char_val = val;
    return e;
}

ASTExpr *ast_expr_str_lit(const char *val, int line, int col) {
    ASTExpr *e = xmalloc(sizeof(ASTExpr));
    e->kind = EXPR_STR_LIT;
    e->eval_type = TYPE_UNKNOWN; // string literal
    e->line = line;
    e->col = col;
    e->str_val = xstrdup(val);
    return e;
}

ASTExpr *ast_expr_var(const char *name, int line, int col) {
    ASTExpr *e = xmalloc(sizeof(ASTExpr));
    e->kind = EXPR_VAR;
    e->eval_type = TYPE_UNKNOWN;
    e->line = line;
    e->col = col;
    e->var_name = xstrdup(name);
    return e;
}

ASTExpr *ast_expr_unary(UnaryOp op, ASTExpr *operand, int line, int col) {
    ASTExpr *e = xmalloc(sizeof(ASTExpr));
    e->kind = EXPR_UNARY;
    e->eval_type = TYPE_UNKNOWN;
    e->line = line;
    e->col = col;
    e->unary.op = op;
    e->unary.operand = operand;
    return e;
}

ASTExpr *ast_expr_binary(BinaryOp op, ASTExpr *left, ASTExpr *right, int line, int col) {
    ASTExpr *e = xmalloc(sizeof(ASTExpr));
    e->kind = EXPR_BINARY;
    e->eval_type = TYPE_UNKNOWN;
    e->line = line;
    e->col = col;
    e->binary.op = op;
    e->binary.left = left;
    e->binary.right = right;
    return e;
}

ASTExpr *ast_expr_assign(const char *var_name, ASTExpr *value, int line, int col) {
    ASTExpr *e = xmalloc(sizeof(ASTExpr));
    e->kind = EXPR_ASSIGN;
    e->eval_type = TYPE_UNKNOWN;
    e->line = line;
    e->col = col;
    e->assign.var_name = xstrdup(var_name);
    e->assign.value = value;
    return e;
}

ASTExpr *ast_expr_call(const char *func_name, ASTExpr **args, int arg_count, int line, int col) {
    ASTExpr *e = xmalloc(sizeof(ASTExpr));
    e->kind = EXPR_CALL;
    e->eval_type = TYPE_UNKNOWN;
    e->line = line;
    e->col = col;
    e->call.func_name = xstrdup(func_name);
    e->call.args = args;
    e->call.arg_count = arg_count;
    return e;
}

ASTExpr *ast_expr_cast(DataType target_type, ASTExpr *expr, int line, int col) {
    ASTExpr *e = xmalloc(sizeof(ASTExpr));
    e->kind = EXPR_CAST;
    e->eval_type = target_type;
    e->line = line;
    e->col = col;
    e->cast.target_type = target_type;
    e->cast.expr = expr;
    return e;
}

ASTStmt *ast_stmt_decl(DataType type, const char *var_name, ASTExpr *init, int line, int col) {
    ASTStmt *s = xmalloc(sizeof(ASTStmt));
    s->kind = STMT_DECL;
    s->line = line;
    s->col = col;
    s->decl.type = type;
    s->decl.var_name = xstrdup(var_name);
    s->decl.init = init;
    return s;
}

ASTStmt *ast_stmt_expr(ASTExpr *expr, int line, int col) {
    ASTStmt *s = xmalloc(sizeof(ASTStmt));
    s->kind = STMT_EXPR;
    s->line = line;
    s->col = col;
    s->expr = expr;
    return s;
}

ASTStmt *ast_stmt_block(ASTStmt **stmts, int stmt_count, int line, int col) {
    ASTStmt *s = xmalloc(sizeof(ASTStmt));
    s->kind = STMT_BLOCK;
    s->line = line;
    s->col = col;
    s->block.stmts = stmts;
    s->block.stmt_count = stmt_count;
    return s;
}

ASTStmt *ast_stmt_if(ASTExpr *cond, ASTStmt *then_branch, ASTStmt *else_branch, int line, int col) {
    ASTStmt *s = xmalloc(sizeof(ASTStmt));
    s->kind = STMT_IF;
    s->line = line;
    s->col = col;
    s->if_stmt.cond = cond;
    s->if_stmt.then_branch = then_branch;
    s->if_stmt.else_branch = else_branch;
    return s;
}

ASTStmt *ast_stmt_while(ASTExpr *cond, ASTStmt *body, int line, int col) {
    ASTStmt *s = xmalloc(sizeof(ASTStmt));
    s->kind = STMT_WHILE;
    s->line = line;
    s->col = col;
    s->while_stmt.cond = cond;
    s->while_stmt.body = body;
    return s;
}

ASTStmt *ast_stmt_for(ASTStmt *init, ASTExpr *cond, ASTExpr *step, ASTStmt *body, int line, int col) {
    ASTStmt *s = xmalloc(sizeof(ASTStmt));
    s->kind = STMT_FOR;
    s->line = line;
    s->col = col;
    s->for_stmt.init = init;
    s->for_stmt.cond = cond;
    s->for_stmt.step = step;
    s->for_stmt.body = body;
    return s;
}

ASTStmt *ast_stmt_return(ASTExpr *value, int line, int col) {
    ASTStmt *s = xmalloc(sizeof(ASTStmt));
    s->kind = STMT_RETURN;
    s->line = line;
    s->col = col;
    s->return_stmt.value = value;
    return s;
}

ASTStmt *ast_stmt_break(int line, int col) {
    ASTStmt *s = xmalloc(sizeof(ASTStmt));
    s->kind = STMT_BREAK;
    s->line = line;
    s->col = col;
    return s;
}

ASTStmt *ast_stmt_continue(int line, int col) {
    ASTStmt *s = xmalloc(sizeof(ASTStmt));
    s->kind = STMT_CONTINUE;
    s->line = line;
    s->col = col;
    return s;
}

ASTProgram *ast_program_new(void) {
    ASTProgram *p = xmalloc(sizeof(ASTProgram));
    p->functions = NULL;
    p->function_count = 0;
    return p;
}

void ast_program_add_func(ASTProgram *prog, ASTFunction *func) {
    prog->function_count++;
    prog->functions = xrealloc(prog->functions, sizeof(ASTFunction*) * prog->function_count);
    prog->functions[prog->function_count - 1] = func;
}

// AST Pretty Printing
static void dump_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

static void dump_expr(ASTExpr *expr, int indent) {
    if (!expr) return;
    dump_indent(indent);
    printf("Expr [type=%s]: ", data_type_to_string(expr->eval_type));
    switch (expr->kind) {
        case EXPR_INT_LIT:
            printf("IntLiteral(%ld)\n", expr->int_val);
            break;
        case EXPR_FLOAT_LIT:
            printf("FloatLiteral(%g)\n", expr->float_val);
            break;
        case EXPR_CHAR_LIT:
            printf("CharLiteral('%c')\n", expr->char_val);
            break;
        case EXPR_STR_LIT:
            printf("StringLiteral(\"%s\")\n", expr->str_val);
            break;
        case EXPR_VAR:
            printf("Var(%s)\n", expr->var_name);
            break;
        case EXPR_UNARY:
            printf("UnaryOp(%s)\n", unary_op_to_string(expr->unary.op));
            dump_expr(expr->unary.operand, indent + 1);
            break;
        case EXPR_BINARY:
            printf("BinaryOp(%s)\n", binary_op_to_string(expr->binary.op));
            dump_expr(expr->binary.left, indent + 1);
            dump_expr(expr->binary.right, indent + 1);
            break;
        case EXPR_ASSIGN:
            printf("Assign(%s =)\n", expr->assign.var_name);
            dump_expr(expr->assign.value, indent + 1);
            break;
        case EXPR_CALL:
            printf("Call(%s, args=%d)\n", expr->call.func_name, expr->call.arg_count);
            for (int i = 0; i < expr->call.arg_count; i++) {
                dump_expr(expr->call.args[i], indent + 1);
            }
            break;
        case EXPR_CAST:
            printf("Cast(to %s)\n", data_type_to_string(expr->cast.target_type));
            dump_expr(expr->cast.expr, indent + 1);
            break;
    }
}

static void dump_stmt(ASTStmt *stmt, int indent) {
    if (!stmt) return;
    dump_indent(indent);
    switch (stmt->kind) {
        case STMT_DECL:
            printf("DeclStmt: %s %s\n", data_type_to_string(stmt->decl.type), stmt->decl.var_name);
            if (stmt->decl.init) {
                dump_expr(stmt->decl.init, indent + 1);
            }
            break;
        case STMT_EXPR:
            printf("ExprStmt:\n");
            dump_expr(stmt->expr, indent + 1);
            break;
        case STMT_BLOCK:
            printf("BlockStmt (%d stmts):\n", stmt->block.stmt_count);
            for (int i = 0; i < stmt->block.stmt_count; i++) {
                dump_stmt(stmt->block.stmts[i], indent + 1);
            }
            break;
        case STMT_IF:
            printf("IfStmt:\n");
            dump_indent(indent + 1); printf("Condition:\n");
            dump_expr(stmt->if_stmt.cond, indent + 2);
            dump_indent(indent + 1); printf("Then:\n");
            dump_stmt(stmt->if_stmt.then_branch, indent + 2);
            if (stmt->if_stmt.else_branch) {
                dump_indent(indent + 1); printf("Else:\n");
                dump_stmt(stmt->if_stmt.else_branch, indent + 2);
            }
            break;
        case STMT_WHILE:
            printf("WhileStmt:\n");
            dump_indent(indent + 1); printf("Condition:\n");
            dump_expr(stmt->while_stmt.cond, indent + 2);
            dump_indent(indent + 1); printf("Body:\n");
            dump_stmt(stmt->while_stmt.body, indent + 2);
            break;
        case STMT_FOR:
            printf("ForStmt:\n");
            if (stmt->for_stmt.init) {
                dump_indent(indent + 1); printf("Init:\n");
                dump_stmt(stmt->for_stmt.init, indent + 2);
            }
            if (stmt->for_stmt.cond) {
                dump_indent(indent + 1); printf("Condition:\n");
                dump_expr(stmt->for_stmt.cond, indent + 2);
            }
            if (stmt->for_stmt.step) {
                dump_indent(indent + 1); printf("Step:\n");
                dump_expr(stmt->for_stmt.step, indent + 2);
            }
            dump_indent(indent + 1); printf("Body:\n");
            dump_stmt(stmt->for_stmt.body, indent + 2);
            break;
        case STMT_RETURN:
            printf("ReturnStmt:\n");
            if (stmt->return_stmt.value) {
                dump_expr(stmt->return_stmt.value, indent + 1);
            }
            break;
        case STMT_BREAK:
            printf("BreakStmt\n");
            break;
        case STMT_CONTINUE:
            printf("ContinueStmt\n");
            break;
    }
}

// AST Freeing
static void free_expr(ASTExpr *expr) {
    if (!expr) return;
    switch (expr->kind) {
        case EXPR_STR_LIT:
            free(expr->str_val);
            break;
        case EXPR_VAR:
            free(expr->var_name);
            break;
        case EXPR_UNARY:
            free_expr(expr->unary.operand);
            break;
        case EXPR_BINARY:
            free_expr(expr->binary.left);
            free_expr(expr->binary.right);
            break;
        case EXPR_ASSIGN:
            free(expr->assign.var_name);
            free_expr(expr->assign.value);
            break;
        case EXPR_CALL:
            free(expr->call.func_name);
            for (int i = 0; i < expr->call.arg_count; i++) {
                free_expr(expr->call.args[i]);
            }
            free(expr->call.args);
            break;
        case EXPR_CAST:
            free_expr(expr->cast.expr);
            break;
        default:
            break;
    }
    free(expr);
}

static void free_stmt(ASTStmt *stmt) {
    if (!stmt) return;
    switch (stmt->kind) {
        case STMT_DECL:
            free(stmt->decl.var_name);
            free_expr(stmt->decl.init);
            break;
        case STMT_EXPR:
            free_expr(stmt->expr);
            break;
        case STMT_BLOCK:
            for (int i = 0; i < stmt->block.stmt_count; i++) {
                free_stmt(stmt->block.stmts[i]);
            }
            free(stmt->block.stmts);
            break;
        case STMT_IF:
            free_expr(stmt->if_stmt.cond);
            free_stmt(stmt->if_stmt.then_branch);
            free_stmt(stmt->if_stmt.else_branch);
            break;
        case STMT_WHILE:
            free_expr(stmt->while_stmt.cond);
            free_stmt(stmt->while_stmt.body);
            break;
        case STMT_FOR:
            free_stmt(stmt->for_stmt.init);
            free_expr(stmt->for_stmt.cond);
            free_expr(stmt->for_stmt.step);
            free_stmt(stmt->for_stmt.body);
            break;
        case STMT_RETURN:
            free_expr(stmt->return_stmt.value);
            break;
        default:
            break;
    }
    free(stmt);
}

void ast_free_program(ASTProgram *prog) {
    if (!prog) return;
    for (int i = 0; i < prog->function_count; i++) {
        ASTFunction *fn = prog->functions[i];
        free(fn->name);
        for (int p = 0; p < fn->param_count; p++) {
            free(fn->params[p].name);
        }
        free(fn->params);
        free_stmt(fn->body);
        free(fn);
    }
    free(prog->functions);
    free(prog);
}

void ast_dump_program(ASTProgram *prog) {
    printf("=== AST DUMP ===\n");
    if (!prog) return;
    for (int i = 0; i < prog->function_count; i++) {
        ASTFunction *fn = prog->functions[i];
        printf("Function %s %s(", data_type_to_string(fn->return_type), fn->name);
        for (int p = 0; p < fn->param_count; p++) {
            printf("%s %s%s", data_type_to_string(fn->params[p].type), fn->params[p].name, p + 1 < fn->param_count ? ", " : "");
        }
        printf(")\n");
        dump_stmt(fn->body, 1);
    }
}
