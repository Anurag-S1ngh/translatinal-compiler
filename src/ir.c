#include "ir.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct LoopContext {
    int continue_label;
    int break_label;
    struct LoopContext *parent;
} LoopContext;

typedef struct {
    IRFunc *current_func;
    int next_temp;
    int next_label;
    LoopContext *current_loop;
} IRGenerator;

IROperand ir_op_none(void) {
    IROperand op;
    op.kind = OPERAND_NONE;
    return op;
}

IROperand ir_op_var(const char *name) {
    IROperand op;
    op.kind = OPERAND_VAR;
    op.name = xstrdup(name);
    return op;
}

IROperand ir_op_temp(int temp_id) {
    IROperand op;
    op.kind = OPERAND_TEMP;
    char buf[32];
    snprintf(buf, sizeof(buf), "%%t%d", temp_id);
    op.name = xstrdup(buf);
    return op;
}

IROperand ir_op_int(long val) {
    IROperand op;
    op.kind = OPERAND_INT_LIT;
    op.int_val = val;
    return op;
}

IROperand ir_op_float(double val) {
    IROperand op;
    op.kind = OPERAND_FLOAT_LIT;
    op.float_val = val;
    return op;
}

IROperand ir_op_char(char val) {
    IROperand op;
    op.kind = OPERAND_CHAR_LIT;
    op.char_val = val;
    return op;
}

IROperand ir_op_str(const char *val) {
    IROperand op;
    op.kind = OPERAND_STR_LIT;
    op.str_val = xstrdup(val);
    return op;
}

IROperand ir_op_label(int label_id) {
    IROperand op;
    op.kind = OPERAND_LABEL;
    char buf[32];
    snprintf(buf, sizeof(buf), ".L%d", label_id);
    op.name = xstrdup(buf);
    return op;
}

static IROperand new_temp(IRGenerator *gen) {
    return ir_op_temp(gen->next_temp++);
}

static int new_label(IRGenerator *gen) {
    return gen->next_label++;
}

static void emit_quad(IRGenerator *gen, IROpcode op, IROperand arg1, IROperand arg2, IROperand result) {
    IRFunc *f = gen->current_func;
    if (f->quad_count + 1 > f->quad_capacity) {
        f->quad_capacity = f->quad_capacity == 0 ? 16 : f->quad_capacity * 2;
        f->quads = xrealloc(f->quads, sizeof(IRQuad) * f->quad_capacity);
    }
    IRQuad q;
    q.op = op;
    q.arg1 = arg1;
    q.arg2 = arg2;
    q.result = result;
    f->quads[f->quad_count++] = q;
}

static IROperand gen_expr(IRGenerator *gen, ASTExpr *expr);
static void gen_stmt(IRGenerator *gen, ASTStmt *stmt);

static IROperand gen_expr(IRGenerator *gen, ASTExpr *expr) {
    if (!expr) return ir_op_none();

    switch (expr->kind) {
        case EXPR_INT_LIT:
            return ir_op_int(expr->int_val);
        case EXPR_FLOAT_LIT:
            return ir_op_float(expr->float_val);
        case EXPR_CHAR_LIT:
            return ir_op_char(expr->char_val);
        case EXPR_STR_LIT:
            return ir_op_str(expr->str_val);
        case EXPR_VAR:
            return ir_op_var(expr->var_name);
        case EXPR_CAST: {
            IROperand sub = gen_expr(gen, expr->cast.expr);
            IROperand res = new_temp(gen);
            if (expr->cast.target_type == TYPE_FLOAT) {
                emit_quad(gen, IR_CAST_INT_TO_FLOAT, sub, ir_op_none(), res);
            } else {
                emit_quad(gen, IR_CAST_FLOAT_TO_INT, sub, ir_op_none(), res);
            }
            return res;
        }
        case EXPR_UNARY: {
            if (expr->unary.op == UNARY_PRE_INC || expr->unary.op == UNARY_PRE_DEC) {
                IROperand var = ir_op_var(expr->unary.operand->var_name);
                IROpcode op = (expr->unary.op == UNARY_PRE_INC) ? IR_ADD : IR_SUB;
                emit_quad(gen, op, var, ir_op_int(1), var);
                return var;
            }
            if (expr->unary.op == UNARY_POST_INC || expr->unary.op == UNARY_POST_DEC) {
                IROperand var = ir_op_var(expr->unary.operand->var_name);
                IROperand temp = new_temp(gen);
                emit_quad(gen, IR_ASSIGN, var, ir_op_none(), temp);
                IROpcode op = (expr->unary.op == UNARY_POST_INC) ? IR_ADD : IR_SUB;
                emit_quad(gen, op, var, ir_op_int(1), var);
                return temp;
            }
            IROperand sub = gen_expr(gen, expr->unary.operand);
            IROperand res = new_temp(gen);
            IROpcode op = (expr->unary.op == UNARY_NEG) ? IR_NEG : IR_NOT;
            emit_quad(gen, op, sub, ir_op_none(), res);
            return res;
        }
        case EXPR_BINARY: {
            // Short circuit logic for && and ||
            if (expr->binary.op == BIN_AND) {
                int false_label = new_label(gen);
                int end_label = new_label(gen);
                IROperand res = new_temp(gen);

                IROperand left = gen_expr(gen, expr->binary.left);
                emit_quad(gen, IR_JUMP_IF_ZERO, left, ir_op_none(), ir_op_label(false_label));

                IROperand right = gen_expr(gen, expr->binary.right);
                emit_quad(gen, IR_JUMP_IF_ZERO, right, ir_op_none(), ir_op_label(false_label));

                emit_quad(gen, IR_ASSIGN, ir_op_int(1), ir_op_none(), res);
                emit_quad(gen, IR_JUMP, ir_op_label(end_label), ir_op_none(), ir_op_none());

                emit_quad(gen, IR_LABEL, ir_op_label(false_label), ir_op_none(), ir_op_none());
                emit_quad(gen, IR_ASSIGN, ir_op_int(0), ir_op_none(), res);

                emit_quad(gen, IR_LABEL, ir_op_label(end_label), ir_op_none(), ir_op_none());
                return res;
            }
            if (expr->binary.op == BIN_OR) {
                int true_label = new_label(gen);
                int end_label = new_label(gen);
                IROperand res = new_temp(gen);

                IROperand left = gen_expr(gen, expr->binary.left);
                emit_quad(gen, IR_JUMP_IF_NZERO, left, ir_op_none(), ir_op_label(true_label));

                IROperand right = gen_expr(gen, expr->binary.right);
                emit_quad(gen, IR_JUMP_IF_NZERO, right, ir_op_none(), ir_op_label(true_label));

                emit_quad(gen, IR_ASSIGN, ir_op_int(0), ir_op_none(), res);
                emit_quad(gen, IR_JUMP, ir_op_label(end_label), ir_op_none(), ir_op_none());

                emit_quad(gen, IR_LABEL, ir_op_label(true_label), ir_op_none(), ir_op_none());
                emit_quad(gen, IR_ASSIGN, ir_op_int(1), ir_op_none(), res);

                emit_quad(gen, IR_LABEL, ir_op_label(end_label), ir_op_none(), ir_op_none());
                return res;
            }

            IROperand left = gen_expr(gen, expr->binary.left);
            IROperand right = gen_expr(gen, expr->binary.right);
            IROperand res = new_temp(gen);

            IROpcode op = IR_ADD;
            switch (expr->binary.op) {
                case BIN_ADD: op = IR_ADD; break;
                case BIN_SUB: op = IR_SUB; break;
                case BIN_MUL: op = IR_MUL; break;
                case BIN_DIV: op = IR_DIV; break;
                case BIN_MOD: op = IR_MOD; break;
                case BIN_EQ:  op = IR_EQ; break;
                case BIN_NEQ: op = IR_NEQ; break;
                case BIN_LT:  op = IR_LT; break;
                case BIN_LTE: op = IR_LTE; break;
                case BIN_GT:  op = IR_GT; break;
                case BIN_GTE: op = IR_GTE; break;
                default: break;
            }
            emit_quad(gen, op, left, right, res);
            return res;
        }
        case EXPR_ASSIGN: {
            IROperand val = gen_expr(gen, expr->assign.value);
            IROperand var = ir_op_var(expr->assign.var_name);
            emit_quad(gen, IR_ASSIGN, val, ir_op_none(), var);
            return var;
        }
        case EXPR_CALL: {
            IROperand *args = xmalloc(sizeof(IROperand) * expr->call.arg_count);
            for (int i = 0; i < expr->call.arg_count; i++) {
                args[i] = gen_expr(gen, expr->call.args[i]);
            }
            for (int i = 0; i < expr->call.arg_count; i++) {
                emit_quad(gen, IR_PARAM, args[i], ir_op_none(), ir_op_none());
            }
            free(args);

            IROperand res = new_temp(gen);
            emit_quad(gen, IR_CALL, ir_op_var(expr->call.func_name), ir_op_int(expr->call.arg_count), res);
            return res;
        }
    }
    return ir_op_none();
}

static void gen_stmt(IRGenerator *gen, ASTStmt *stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case STMT_DECL: {
            if (stmt->decl.init) {
                IROperand init_val = gen_expr(gen, stmt->decl.init);
                emit_quad(gen, IR_ASSIGN, init_val, ir_op_none(), ir_op_var(stmt->decl.var_name));
            }
            break;
        }
        case STMT_EXPR:
            gen_expr(gen, stmt->expr);
            break;
        case STMT_BLOCK:
            for (int i = 0; i < stmt->block.stmt_count; i++) {
                gen_stmt(gen, stmt->block.stmts[i]);
            }
            break;
        case STMT_IF: {
            int else_label = new_label(gen);
            int end_label = new_label(gen);

            IROperand cond = gen_expr(gen, stmt->if_stmt.cond);
            emit_quad(gen, IR_JUMP_IF_ZERO, cond, ir_op_none(), ir_op_label(else_label));

            gen_stmt(gen, stmt->if_stmt.then_branch);
            emit_quad(gen, IR_JUMP, ir_op_label(end_label), ir_op_none(), ir_op_none());

            emit_quad(gen, IR_LABEL, ir_op_label(else_label), ir_op_none(), ir_op_none());
            if (stmt->if_stmt.else_branch) {
                gen_stmt(gen, stmt->if_stmt.else_branch);
            }
            emit_quad(gen, IR_LABEL, ir_op_label(end_label), ir_op_none(), ir_op_none());
            break;
        }
        case STMT_WHILE: {
            int start_label = new_label(gen);
            int end_label = new_label(gen);

            LoopContext lc;
            lc.continue_label = start_label;
            lc.break_label = end_label;
            lc.parent = gen->current_loop;
            gen->current_loop = &lc;

            emit_quad(gen, IR_LABEL, ir_op_label(start_label), ir_op_none(), ir_op_none());
            IROperand cond = gen_expr(gen, stmt->while_stmt.cond);
            emit_quad(gen, IR_JUMP_IF_ZERO, cond, ir_op_none(), ir_op_label(end_label));

            gen_stmt(gen, stmt->while_stmt.body);
            emit_quad(gen, IR_JUMP, ir_op_label(start_label), ir_op_none(), ir_op_none());

            emit_quad(gen, IR_LABEL, ir_op_label(end_label), ir_op_none(), ir_op_none());

            gen->current_loop = lc.parent;
            break;
        }
        case STMT_FOR: {
            int start_label = new_label(gen);
            int step_label = new_label(gen);
            int end_label = new_label(gen);

            if (stmt->for_stmt.init) {
                gen_stmt(gen, stmt->for_stmt.init);
            }

            LoopContext lc;
            lc.continue_label = step_label;
            lc.break_label = end_label;
            lc.parent = gen->current_loop;
            gen->current_loop = &lc;

            emit_quad(gen, IR_LABEL, ir_op_label(start_label), ir_op_none(), ir_op_none());
            if (stmt->for_stmt.cond) {
                IROperand cond = gen_expr(gen, stmt->for_stmt.cond);
                emit_quad(gen, IR_JUMP_IF_ZERO, cond, ir_op_none(), ir_op_label(end_label));
            }

            gen_stmt(gen, stmt->for_stmt.body);

            emit_quad(gen, IR_LABEL, ir_op_label(step_label), ir_op_none(), ir_op_none());
            if (stmt->for_stmt.step) {
                gen_expr(gen, stmt->for_stmt.step);
            }
            emit_quad(gen, IR_JUMP, ir_op_label(start_label), ir_op_none(), ir_op_none());

            emit_quad(gen, IR_LABEL, ir_op_label(end_label), ir_op_none(), ir_op_none());

            gen->current_loop = lc.parent;
            break;
        }
        case STMT_RETURN: {
            IROperand val = ir_op_none();
            if (stmt->return_stmt.value) {
                val = gen_expr(gen, stmt->return_stmt.value);
            }
            emit_quad(gen, IR_RETURN, val, ir_op_none(), ir_op_none());
            break;
        }
        case STMT_BREAK:
            if (gen->current_loop) {
                emit_quad(gen, IR_JUMP, ir_op_label(gen->current_loop->break_label), ir_op_none(), ir_op_none());
            }
            break;
        case STMT_CONTINUE:
            if (gen->current_loop) {
                emit_quad(gen, IR_JUMP, ir_op_label(gen->current_loop->continue_label), ir_op_none(), ir_op_none());
            }
            break;
    }
}

IRProgram *ir_generate(ASTProgram *ast) {
    IRProgram *prog = xmalloc(sizeof(IRProgram));
    prog->funcs = NULL;
    prog->func_count = 0;

    IRGenerator gen;
    gen.next_temp = 0;
    gen.next_label = 0;
    gen.current_loop = NULL;

    for (int i = 0; i < ast->function_count; i++) {
        ASTFunction *ast_fn = ast->functions[i];

        IRFunc *fn = xmalloc(sizeof(IRFunc));
        fn->name = xstrdup(ast_fn->name);
        fn->return_type = ast_fn->return_type;
        fn->quads = NULL;
        fn->quad_count = 0;
        fn->quad_capacity = 0;

        gen.current_func = fn;
        gen.next_temp = 0;

        emit_quad(&gen, IR_FUNC_START, ir_op_var(ast_fn->name), ir_op_none(), ir_op_none());
        if (ast_fn->body) {
            gen_stmt(&gen, ast_fn->body);
        }
        emit_quad(&gen, IR_FUNC_END, ir_op_var(ast_fn->name), ir_op_none(), ir_op_none());

        prog->func_count++;
        prog->funcs = xrealloc(prog->funcs, sizeof(IRFunc*) * prog->func_count);
        prog->funcs[prog->func_count - 1] = fn;
    }

    return prog;
}

static void print_operand(IROperand op) {
    switch (op.kind) {
        case OPERAND_NONE: break;
        case OPERAND_VAR: printf("%s", op.name); break;
        case OPERAND_TEMP: printf("%s", op.name); break;
        case OPERAND_INT_LIT: printf("%ld", op.int_val); break;
        case OPERAND_FLOAT_LIT: printf("%g", op.float_val); break;
        case OPERAND_CHAR_LIT: printf("'%c'", op.char_val); break;
        case OPERAND_STR_LIT: printf("\"%s\"", op.str_val); break;
        case OPERAND_LABEL: printf("%s", op.name); break;
    }
}

void ir_dump_program(IRProgram *prog) {
    printf("=== THREE-ADDRESS CODE IR ===\n");
    if (!prog) return;

    for (int f = 0; f < prog->func_count; f++) {
        IRFunc *fn = prog->funcs[f];
        for (int i = 0; i < fn->quad_count; i++) {
            IRQuad q = fn->quads[i];
            switch (q.op) {
                case IR_FUNC_START:
                    printf("\nFUNC %s:\n", q.arg1.name);
                    break;
                case IR_FUNC_END:
                    printf("END FUNC %s\n", q.arg1.name);
                    break;
                case IR_LABEL:
                    printf("%s:\n", q.arg1.name);
                    break;
                case IR_ASSIGN:
                    printf("  "); print_operand(q.result); printf(" = "); print_operand(q.arg1); printf("\n");
                    break;
                case IR_ADD:
                    printf("  "); print_operand(q.result); printf(" = "); print_operand(q.arg1); printf(" + "); print_operand(q.arg2); printf("\n");
                    break;
                case IR_SUB:
                    printf("  "); print_operand(q.result); printf(" = "); print_operand(q.arg1); printf(" - "); print_operand(q.arg2); printf("\n");
                    break;
                case IR_MUL:
                    printf("  "); print_operand(q.result); printf(" = "); print_operand(q.arg1); printf(" * "); print_operand(q.arg2); printf("\n");
                    break;
                case IR_DIV:
                    printf("  "); print_operand(q.result); printf(" = "); print_operand(q.arg1); printf(" / "); print_operand(q.arg2); printf("\n");
                    break;
                case IR_MOD:
                    printf("  "); print_operand(q.result); printf(" = "); print_operand(q.arg1); printf(" %% "); print_operand(q.arg2); printf("\n");
                    break;
                case IR_NEG:
                    printf("  "); print_operand(q.result); printf(" = -"); print_operand(q.arg1); printf("\n");
                    break;
                case IR_NOT:
                    printf("  "); print_operand(q.result); printf(" = !"); print_operand(q.arg1); printf("\n");
                    break;
                case IR_EQ:
                    printf("  "); print_operand(q.result); printf(" = "); print_operand(q.arg1); printf(" == "); print_operand(q.arg2); printf("\n");
                    break;
                case IR_NEQ:
                    printf("  "); print_operand(q.result); printf(" = "); print_operand(q.arg1); printf(" != "); print_operand(q.arg2); printf("\n");
                    break;
                case IR_LT:
                    printf("  "); print_operand(q.result); printf(" = "); print_operand(q.arg1); printf(" < "); print_operand(q.arg2); printf("\n");
                    break;
                case IR_LTE:
                    printf("  "); print_operand(q.result); printf(" = "); print_operand(q.arg1); printf(" <= "); print_operand(q.arg2); printf("\n");
                    break;
                case IR_GT:
                    printf("  "); print_operand(q.result); printf(" = "); print_operand(q.arg1); printf(" > "); print_operand(q.arg2); printf("\n");
                    break;
                case IR_GTE:
                    printf("  "); print_operand(q.result); printf(" = "); print_operand(q.arg1); printf(" >= "); print_operand(q.arg2); printf("\n");
                    break;
                case IR_CAST_INT_TO_FLOAT:
                    printf("  "); print_operand(q.result); printf(" = (float)"); print_operand(q.arg1); printf("\n");
                    break;
                case IR_CAST_FLOAT_TO_INT:
                    printf("  "); print_operand(q.result); printf(" = (int)"); print_operand(q.arg1); printf("\n");
                    break;
                case IR_JUMP:
                    printf("  goto %s\n", q.arg1.name);
                    break;
                case IR_JUMP_IF_ZERO:
                    printf("  ifz "); print_operand(q.arg1); printf(" goto %s\n", q.result.name);
                    break;
                case IR_JUMP_IF_NZERO:
                    printf("  ifnz "); print_operand(q.arg1); printf(" goto %s\n", q.result.name);
                    break;
                case IR_PARAM:
                    printf("  param "); print_operand(q.arg1); printf("\n");
                    break;
                case IR_CALL:
                    printf("  "); print_operand(q.result); printf(" = call %s, %ld\n", q.arg1.name, q.arg2.int_val);
                    break;
                case IR_RETURN:
                    printf("  return "); print_operand(q.arg1); printf("\n");
                    break;
            }
        }
    }
}
