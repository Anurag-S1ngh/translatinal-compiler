#include "codegen.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

static void emit_indent(FILE *out, int level) {
    for (int i = 0; i < level; i++) {
        fputs("    ", out);
    }
}

static const char *bin_op_text(BinaryOp op) {
    switch (op) {
        case BIN_ADD: return "+";
        case BIN_SUB: return "-";
        case BIN_MUL: return "*";
        case BIN_DIV: return "/";
        case BIN_MOD: return "%";
        case BIN_EQ:  return "==";
        case BIN_NEQ: return "!=";
        case BIN_LT:  return "<";
        case BIN_LTE: return "<=";
        case BIN_GT:  return ">";
        case BIN_GTE: return ">=";
        case BIN_AND: return "&&";
        case BIN_OR:  return "||";
    }
    return "?";
}

static void emit_escaped_string(FILE *out, const char *s) {
    fputc('"', out);
    if (s) {
        for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
            switch (*p) {
                case '\n': fputs("\\n", out); break;
                case '\t': fputs("\\t", out); break;
                case '\r': fputs("\\r", out); break;
                case '\\': fputs("\\\\", out); break;
                case '"':  fputs("\\\"", out); break;
                default:
                    if (*p < 32 || *p > 126) {
                        fprintf(out, "\\%03o", *p);
                    } else {
                        fputc(*p, out);
                    }
                    break;
            }
        }
    }
    fputc('"', out);
}

static void emit_escaped_char(FILE *out, char c) {
    fputc('\'', out);
    switch (c) {
        case '\n': fputs("\\n", out); break;
        case '\t': fputs("\\t", out); break;
        case '\r': fputs("\\r", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\'': fputs("\\'", out); break;
        case '\0': fputs("\\0", out); break;
        default:
            if (c < 32 || c > 126) {
                fprintf(out, "\\%03o", (unsigned char)c);
            } else {
                fputc(c, out);
            }
            break;
    }
    fputc('\'', out);
}

// `top` is true only for an expression emitted directly in a
// parenthesized context position (if/while/for condition), where adding
// redundant outer parentheses would trip -Wparentheses-equality.
static void emit_expr_impl(FILE *out, ASTExpr *e, bool top);

static void emit_expr(FILE *out, ASTExpr *e) {
    emit_expr_impl(out, e, false);
}

static void emit_expr_top(FILE *out, ASTExpr *e) {
    emit_expr_impl(out, e, true);
}

static void emit_expr_impl(FILE *out, ASTExpr *e, bool top) {
    if (!e) return;
    switch (e->kind) {
        case EXPR_INT_LIT:
            fprintf(out, "%ld", e->int_val);
            break;
        case EXPR_FLOAT_LIT:
            // %g round-trips through gcc; append nothing, sema already inserted casts.
            fprintf(out, "%g", e->float_val);
            break;
        case EXPR_CHAR_LIT:
            emit_escaped_char(out, e->char_val);
            break;
        case EXPR_STR_LIT:
            emit_escaped_string(out, e->str_val);
            break;
        case EXPR_VAR:
            fputs(e->var_name, out);
            break;
        case EXPR_UNARY:
            switch (e->unary.op) {
                case UNARY_NEG: if (!top) fputc('(', out); fputs("-", out); emit_expr(out, e->unary.operand); if (!top) fputc(')', out); break;
                case UNARY_NOT: if (!top) fputc('(', out); fputs("!", out); emit_expr(out, e->unary.operand); if (!top) fputc(')', out); break;
                case UNARY_PRE_INC: if (!top) fputc('(', out); fputs("++", out); emit_expr(out, e->unary.operand); if (!top) fputc(')', out); break;
                case UNARY_PRE_DEC: if (!top) fputc('(', out); fputs("--", out); emit_expr(out, e->unary.operand); if (!top) fputc(')', out); break;
                case UNARY_POST_INC: if (!top) fputc('(', out); emit_expr(out, e->unary.operand); fputs("++", out); if (!top) fputc(')', out); break;
                case UNARY_POST_DEC: if (!top) fputc('(', out); emit_expr(out, e->unary.operand); fputs("--", out); if (!top) fputc(')', out); break;
            }
            break;
        case EXPR_BINARY:
            // Fully parenthesized: immune to precedence/associativity mistakes.
            // && and || keep short-circuit semantics in the target C compiler.
            if (!top) fputc('(', out);
            emit_expr(out, e->binary.left);
            fprintf(out, " %s ", bin_op_text(e->binary.op));
            emit_expr(out, e->binary.right);
            if (!top) fputc(')', out);
            break;
        case EXPR_ASSIGN:
            if (!top) fprintf(out, "(%s = ", e->assign.var_name); else fprintf(out, "%s = ", e->assign.var_name);
            emit_expr(out, e->assign.value);
            if (!top) fputc(')', out);
            break;
        case EXPR_CALL:
            fputs(e->call.func_name, out);
            fputc('(', out);
            for (int i = 0; i < e->call.arg_count; i++) {
                if (i > 0) fputs(", ", out);
                emit_expr(out, e->call.args[i]);
            }
            fputc(')', out);
            break;
        case EXPR_CAST:
            if (!top) fprintf(out, "((%s)", data_type_to_string(e->cast.target_type)); else fprintf(out, "(%s)", data_type_to_string(e->cast.target_type));
            emit_expr(out, e->cast.expr);
            if (!top) fputc(')', out);
            break;
    }
}

static void emit_stmt(FILE *out, ASTStmt *s, int indent);

static void emit_block_inline(FILE *out, ASTStmt *s, int indent) {
    // Emit a statement in "condition position": blocks inline, anything
    // else wrapped in braces so `if (c) x=1;` style ambiguity disappears.
    if (!s) return;
    if (s->kind == STMT_BLOCK) {
        fputs(" {\n", out);
        for (int i = 0; i < s->block.stmt_count; i++) {
            emit_stmt(out, s->block.stmts[i], indent + 1);
        }
        emit_indent(out, indent);
        fputc('}', out);
    } else {
        fputs(" {\n", out);
        emit_stmt(out, s, indent + 1);
        emit_indent(out, indent);
        fputc('}', out);
    }
}

// Emits a for-init clause without its trailing newline/semicolon duplication.
static void emit_for_init(FILE *out, ASTStmt *init) {
    if (!init) return;
    if (init->kind == STMT_DECL) {
        fprintf(out, "%s %s", data_type_to_string(init->decl.type), init->decl.var_name);
        if (init->decl.init) {
            fputs(" = ", out);
            emit_expr(out, init->decl.init);
        }
    } else if (init->kind == STMT_EXPR) {
        emit_expr(out, init->expr);
    }
}

static void emit_stmt(FILE *out, ASTStmt *s, int indent) {
    if (!s) return;
    emit_indent(out, indent);
    switch (s->kind) {
        case STMT_DECL:
            fprintf(out, "%s %s", data_type_to_string(s->decl.type), s->decl.var_name);
            if (s->decl.init) {
                fputs(" = ", out);
                emit_expr(out, s->decl.init);
            }
            fputs(";\n", out);
            break;
        case STMT_EXPR:
            emit_expr(out, s->expr);
            fputs(";\n", out);
            break;
        case STMT_BLOCK:
            fputs("{\n", out);
            for (int i = 0; i < s->block.stmt_count; i++) {
                emit_stmt(out, s->block.stmts[i], indent + 1);
            }
            emit_indent(out, indent);
            fputs("}\n", out);
            break;
        case STMT_IF:
            fputs("if (", out);
            emit_expr_top(out, s->if_stmt.cond);
            fputc(')', out);
            emit_block_inline(out, s->if_stmt.then_branch, indent);
            if (s->if_stmt.else_branch) {
                fputs(" else", out);
                emit_block_inline(out, s->if_stmt.else_branch, indent);
            }
            fputc('\n', out);
            break;
        case STMT_WHILE:
            fputs("while (", out);
            emit_expr_top(out, s->while_stmt.cond);
            fputc(')', out);
            emit_block_inline(out, s->while_stmt.body, indent);
            fputc('\n', out);
            break;
        case STMT_FOR:
            fputs("for (", out);
            emit_for_init(out, s->for_stmt.init);
            fputs("; ", out);
            if (s->for_stmt.cond) emit_expr_top(out, s->for_stmt.cond);
            fputs("; ", out);
            if (s->for_stmt.step) emit_expr(out, s->for_stmt.step);
            fputc(')', out);
            emit_block_inline(out, s->for_stmt.body, indent);
            fputc('\n', out);
            break;
        case STMT_RETURN:
            if (s->return_stmt.value) {
                fputs("return ", out);
                emit_expr(out, s->return_stmt.value);
                fputs(";\n", out);
            } else {
                fputs("return;\n", out);
            }
            break;
        case STMT_BREAK:
            fputs("break;\n", out);
            break;
        case STMT_CONTINUE:
            fputs("continue;\n", out);
            break;
    }
}

void codegen_emit_program(ASTProgram *prog, FILE *out) {
    fprintf(out, "/* Generated by trco: C-to-C translation. Semantically equivalent to source. */\n");
    fprintf(out, "#include <stdio.h>\n\n");
    if (!prog) return;
    for (int i = 0; i < prog->function_count; i++) {
        ASTFunction *fn = prog->functions[i];
        if (i > 0) fputc('\n', out);
        fprintf(out, "%s %s(", data_type_to_string(fn->return_type), fn->name);
        for (int p = 0; p < fn->param_count; p++) {
            if (p > 0) fputs(", ", out);
            fprintf(out, "%s %s", data_type_to_string(fn->params[p].type), fn->params[p].name);
        }
        if (fn->param_count == 0) {
            // Emit explicit void for nullary functions so -Wstrict-prototypes is happy.
            // Only for definitions; `int main()` -> `int main(void)`.
            fputs("void", out);
        }
        fputs(")\n", out);
        emit_stmt(out, fn->body, 0);
    }
}

int codegen_emit_to_file(ASTProgram *prog, const char *path) {
    FILE *out = fopen(path, "w");
    if (!out) return -1;
    codegen_emit_program(prog, out);
    fclose(out);
    return 0;
}
