#include "opt.h"
#include <string.h>

// Fold adjacent statement pairs inside straight-line block sequences:
//
//   T x = e1;  x = e2;   -->   T x = e2;    (e2 does not read x, e1 side-effect-free)
//   x = e1;    x = e2;   -->   x = e2;      (e2 does not read x, e1 side-effect-free)
//   T x;       x = e2;   -->   T x = e2;    (e2 does not read x)
//
// When e2 reads x, the first store is live (e.g. "int a = 5; a = a + 1;")
// and the pair is left untouched. Nothing here crosses a function call or
// a control-flow boundary.

static bool expr_reads_var(const ASTExpr *e, const char *name) {
    if (!e) return false;
    switch (e->kind) {
        case EXPR_VAR:
            return strcmp(e->var_name, name) == 0;
        case EXPR_ASSIGN:
            // A nested assignment to the variable counts as touching it.
            if (strcmp(e->assign.var_name, name) == 0) return true;
            return expr_reads_var(e->assign.value, name);
        case EXPR_UNARY:
            return expr_reads_var(e->unary.operand, name);
        case EXPR_BINARY:
            return expr_reads_var(e->binary.left, name) || expr_reads_var(e->binary.right, name);
        case EXPR_CAST:
            return expr_reads_var(e->cast.expr, name);
        case EXPR_CALL:
            for (int i = 0; i < e->call.arg_count; i++) {
                if (expr_reads_var(e->call.args[i], name)) return true;
            }
            return false;
        default:
            return false; // literals
    }
}

static bool expr_side_effect_free(const ASTExpr *e) {
    if (!e) return true;
    switch (e->kind) {
        case EXPR_INT_LIT:
        case EXPR_FLOAT_LIT:
        case EXPR_CHAR_LIT:
        case EXPR_STR_LIT:
        case EXPR_VAR:
            return true;
        case EXPR_UNARY:
            switch (e->unary.op) {
                case UNARY_PRE_INC:
                case UNARY_PRE_DEC:
                case UNARY_POST_INC:
                case UNARY_POST_DEC:
                    return false; // writes its operand
                default:
                    return expr_side_effect_free(e->unary.operand);
            }
        case EXPR_BINARY:
            return expr_side_effect_free(e->binary.left) && expr_side_effect_free(e->binary.right);
        case EXPR_CAST:
            return expr_side_effect_free(e->cast.expr);
        default:
            return false; // EXPR_ASSIGN, EXPR_CALL
    }
}

// Recognizes `name = <value>` written as an expression statement. The
// returned pointers are borrowed (still owned by stmt).
static bool is_assign_stmt(ASTStmt *stmt, const char **name_out, ASTExpr **value_out) {
    if (!stmt || stmt->kind != STMT_EXPR) return false;
    ASTExpr *e = stmt->expr;
    if (!e || e->kind != EXPR_ASSIGN) return false;
    *name_out = e->assign.var_name;
    *value_out = e->assign.value;
    return true;
}

// Folds (prev, cur) where cur is `x = e2`. On success, prev's slot holds
// the folded statement (or the dead prev is dropped) and cur is consumed.
static bool try_fold(ASTStmt **slot, ASTStmt *cur) {
    ASTStmt *prev = *slot;
    const char *x = NULL;
    ASTExpr *e2 = NULL;
    if (!is_assign_stmt(cur, &x, &e2)) return false;
    if (expr_reads_var(e2, x)) return false; // cur's value depends on the earlier store

    if (prev->kind == STMT_DECL && strcmp(prev->decl.var_name, x) == 0) {
        // Only fold if the discarded initializer has no side effects:
        //   int a = f(); a = 10;   must call f()!
        if (prev->decl.init && !expr_side_effect_free(prev->decl.init)) {
            return false;
        }
        ast_expr_free(prev->decl.init);
        prev->decl.init = e2;      // steal e2's subtree
        cur->expr->assign.value = NULL;
        ast_stmt_free(cur);
        return true;
    }

    const char *px = NULL;
    ASTExpr *e1 = NULL;
    if (is_assign_stmt(prev, &px, &e1) && strcmp(px, x) == 0 && expr_side_effect_free(e1)) {
        // Dead store: x = e1 has no reader before x = e2 overwrites it.
        ast_stmt_free(prev);
        *slot = cur; // cur takes prev's slot
        return true;
    }
    return false;
}

static void opt_stmt(ASTStmt *stmt);

static void opt_block(ASTStmt *block) {
    int n = block->block.stmt_count;
    for (int i = 0; i < n; i++) {
        opt_stmt(block->block.stmts[i]);
    }
    int w = 0;
    for (int r = 0; r < n; r++) {
        ASTStmt *s = block->block.stmts[r];
        if (w > 0 && try_fold(&block->block.stmts[w - 1], s)) {
            continue; // absorbed into the previous slot
        }
        block->block.stmts[w++] = s;
    }
    block->block.stmt_count = w;
}

static void opt_stmt(ASTStmt *stmt) {
    if (!stmt) return;
    switch (stmt->kind) {
        case STMT_BLOCK:
            opt_block(stmt);
            break;
        case STMT_IF:
            opt_stmt(stmt->if_stmt.then_branch);
            opt_stmt(stmt->if_stmt.else_branch);
            break;
        case STMT_WHILE:
            opt_stmt(stmt->while_stmt.body);
            break;
        case STMT_FOR:
            opt_stmt(stmt->for_stmt.body);
            break;
        default:
            break;
    }
}

void optimize_program(ASTProgram *prog) {
    if (!prog) return;
    for (int i = 0; i < prog->function_count; i++) {
        opt_stmt(prog->functions[i]->body);
    }
}
