#include "parser.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void advance_token(Parser *p) {
    p->previous = p->current;
    p->current = lexer_next_token(&p->lexer);
}

static bool check(Parser *p, TokenKind kind) {
    return p->current.kind == kind;
}

static bool match(Parser *p, TokenKind kind) {
    if (check(p, kind)) {
        advance_token(p);
        return true;
    }
    return false;
}

static Token consume(Parser *p, TokenKind kind, const char *message) {
    if (check(p, kind)) {
        Token tok = p->current;
        advance_token(p);
        return tok;
    }
    error_at(p->current.line, p->current.col, "%s (got '%s')", message, p->current.text);
    return p->current;
}

void parser_init(Parser *parser, const char *source) {
    lexer_init(&parser->lexer, source);
    advance_token(parser);
}

static DataType parse_type(Parser *p) {
    if (match(p, TOKEN_INT)) return TYPE_INT;
    if (match(p, TOKEN_FLOAT)) return TYPE_FLOAT;
    if (match(p, TOKEN_CHAR)) return TYPE_CHAR;
    if (match(p, TOKEN_VOID)) return TYPE_VOID;
    error_at(p->current.line, p->current.col, "Expected type specifier (int, float, char, void)");
    return TYPE_UNKNOWN;
}

static bool is_type_specifier(TokenKind kind) {
    return kind == TOKEN_INT || kind == TOKEN_FLOAT || kind == TOKEN_CHAR || kind == TOKEN_VOID;
}

// Forward declarations
static ASTExpr *parse_expr(Parser *p);
static ASTStmt *parse_stmt(Parser *p);
static ASTStmt *parse_block(Parser *p);

static ASTExpr *parse_primary(Parser *p) {
    int line = p->current.line;
    int col = p->current.col;

    if (match(p, TOKEN_INT_LIT)) {
        return ast_expr_int_lit(p->previous.int_val, line, col);
    }
    if (match(p, TOKEN_FLOAT_LIT)) {
        return ast_expr_float_lit(p->previous.float_val, line, col);
    }
    if (match(p, TOKEN_CHAR_LIT)) {
        return ast_expr_char_lit(p->previous.char_val, line, col);
    }
    if (match(p, TOKEN_STR_LIT)) {
        return ast_expr_str_lit(p->previous.str_val, line, col);
    }
    if (match(p, TOKEN_IDENT)) {
        char *name = xstrdup(p->previous.text);
        if (match(p, TOKEN_LPAREN)) {
            // Function call
            ASTExpr **args = NULL;
            int arg_count = 0;
            if (!check(p, TOKEN_RPAREN)) {
                do {
                    ASTExpr *arg = parse_expr(p);
                    arg_count++;
                    args = xrealloc(args, sizeof(ASTExpr*) * arg_count);
                    args[arg_count - 1] = arg;
                } while (match(p, TOKEN_COMMA));
            }
            consume(p, TOKEN_RPAREN, "Expected ')' after function arguments");
            return ast_expr_call(name, args, arg_count, line, col);
        }
        return ast_expr_var(name, line, col);
    }
    if (match(p, TOKEN_LPAREN)) {
        ASTExpr *expr = parse_expr(p);
        consume(p, TOKEN_RPAREN, "Expected ')' after expression");
        return expr;
    }

    error_at(line, col, "Expected expression");
    advance_token(p); // Recover
    return ast_expr_int_lit(0, line, col);
}

static ASTExpr *parse_postfix(Parser *p) {
    int line = p->current.line;
    int col = p->current.col;
    ASTExpr *expr = parse_primary(p);

    while (true) {
        if (match(p, TOKEN_PLUSPLUS)) {
            expr = ast_expr_unary(UNARY_POST_INC, expr, line, col);
        } else if (match(p, TOKEN_MINUSMINUS)) {
            expr = ast_expr_unary(UNARY_POST_DEC, expr, line, col);
        } else {
            break;
        }
    }
    return expr;
}

static ASTExpr *parse_unary(Parser *p) {
    int line = p->current.line;
    int col = p->current.col;

    if (match(p, TOKEN_MINUS)) {
        ASTExpr *operand = parse_unary(p);
        return ast_expr_unary(UNARY_NEG, operand, line, col);
    }
    if (match(p, TOKEN_NOT)) {
        ASTExpr *operand = parse_unary(p);
        return ast_expr_unary(UNARY_NOT, operand, line, col);
    }
    if (match(p, TOKEN_PLUSPLUS)) {
        ASTExpr *operand = parse_unary(p);
        return ast_expr_unary(UNARY_PRE_INC, operand, line, col);
    }
    if (match(p, TOKEN_MINUSMINUS)) {
        ASTExpr *operand = parse_unary(p);
        return ast_expr_unary(UNARY_PRE_DEC, operand, line, col);
    }

    return parse_postfix(p);
}

static ASTExpr *parse_multiplicative(Parser *p) {
    ASTExpr *expr = parse_unary(p);

    while (true) {
        int line = p->current.line;
        int col = p->current.col;
        if (match(p, TOKEN_STAR)) {
            ASTExpr *right = parse_unary(p);
            expr = ast_expr_binary(BIN_MUL, expr, right, line, col);
        } else if (match(p, TOKEN_SLASH)) {
            ASTExpr *right = parse_unary(p);
            expr = ast_expr_binary(BIN_DIV, expr, right, line, col);
        } else if (match(p, TOKEN_PERCENT)) {
            ASTExpr *right = parse_unary(p);
            expr = ast_expr_binary(BIN_MOD, expr, right, line, col);
        } else {
            break;
        }
    }
    return expr;
}

static ASTExpr *parse_additive(Parser *p) {
    ASTExpr *expr = parse_multiplicative(p);

    while (true) {
        int line = p->current.line;
        int col = p->current.col;
        if (match(p, TOKEN_PLUS)) {
            ASTExpr *right = parse_multiplicative(p);
            expr = ast_expr_binary(BIN_ADD, expr, right, line, col);
        } else if (match(p, TOKEN_MINUS)) {
            ASTExpr *right = parse_multiplicative(p);
            expr = ast_expr_binary(BIN_SUB, expr, right, line, col);
        } else {
            break;
        }
    }
    return expr;
}

static ASTExpr *parse_relational(Parser *p) {
    ASTExpr *expr = parse_additive(p);

    while (true) {
        int line = p->current.line;
        int col = p->current.col;
        if (match(p, TOKEN_LT)) {
            ASTExpr *right = parse_additive(p);
            expr = ast_expr_binary(BIN_LT, expr, right, line, col);
        } else if (match(p, TOKEN_LTE)) {
            ASTExpr *right = parse_additive(p);
            expr = ast_expr_binary(BIN_LTE, expr, right, line, col);
        } else if (match(p, TOKEN_GT)) {
            ASTExpr *right = parse_additive(p);
            expr = ast_expr_binary(BIN_GT, expr, right, line, col);
        } else if (match(p, TOKEN_GTE)) {
            ASTExpr *right = parse_additive(p);
            expr = ast_expr_binary(BIN_GTE, expr, right, line, col);
        } else {
            break;
        }
    }
    return expr;
}

static ASTExpr *parse_equality(Parser *p) {
    ASTExpr *expr = parse_relational(p);

    while (true) {
        int line = p->current.line;
        int col = p->current.col;
        if (match(p, TOKEN_EQ)) {
            ASTExpr *right = parse_relational(p);
            expr = ast_expr_binary(BIN_EQ, expr, right, line, col);
        } else if (match(p, TOKEN_NEQ)) {
            ASTExpr *right = parse_relational(p);
            expr = ast_expr_binary(BIN_NEQ, expr, right, line, col);
        } else {
            break;
        }
    }
    return expr;
}

static ASTExpr *parse_logical_and(Parser *p) {
    ASTExpr *expr = parse_equality(p);

    while (match(p, TOKEN_AND)) {
        int line = p->previous.line;
        int col = p->previous.col;
        ASTExpr *right = parse_equality(p);
        expr = ast_expr_binary(BIN_AND, expr, right, line, col);
    }
    return expr;
}

static ASTExpr *parse_logical_or(Parser *p) {
    ASTExpr *expr = parse_logical_and(p);

    while (match(p, TOKEN_OR)) {
        int line = p->previous.line;
        int col = p->previous.col;
        ASTExpr *right = parse_logical_and(p);
        expr = ast_expr_binary(BIN_OR, expr, right, line, col);
    }
    return expr;
}

static ASTExpr *parse_assignment(Parser *p) {
    ASTExpr *expr = parse_logical_or(p);

    if (match(p, TOKEN_ASSIGN)) {
        int line = p->previous.line;
        int col = p->previous.col;
        if (expr->kind != EXPR_VAR) {
            error_at(line, col, "Invalid lvalue in assignment");
        }
        ASTExpr *value = parse_assignment(p); // Right-associative
        char *var_name = expr->kind == EXPR_VAR ? expr->var_name : "invalid";
        return ast_expr_assign(var_name, value, line, col);
    }
    return expr;
}

static ASTExpr *parse_expr(Parser *p) {
    return parse_assignment(p);
}

static ASTStmt *parse_decl_stmt(Parser *p) {
    int line = p->current.line;
    int col = p->current.col;
    DataType type = parse_type(p);
    Token name_tok = consume(p, TOKEN_IDENT, "Expected variable name in declaration");
    ASTExpr *init = NULL;
    if (match(p, TOKEN_ASSIGN)) {
        init = parse_expr(p);
    }
    consume(p, TOKEN_SEMICOLON, "Expected ';' after variable declaration");
    return ast_stmt_decl(type, name_tok.text, init, line, col);
}

static ASTStmt *parse_if_stmt(Parser *p) {
    int line = p->current.line;
    int col = p->current.col;
    consume(p, TOKEN_IF, "Expected 'if'");
    consume(p, TOKEN_LPAREN, "Expected '(' after 'if'");
    ASTExpr *cond = parse_expr(p);
    consume(p, TOKEN_RPAREN, "Expected ')' after 'if' condition");
    ASTStmt *then_branch = parse_stmt(p);
    ASTStmt *else_branch = NULL;
    if (match(p, TOKEN_ELSE)) {
        else_branch = parse_stmt(p);
    }
    return ast_stmt_if(cond, then_branch, else_branch, line, col);
}

static ASTStmt *parse_while_stmt(Parser *p) {
    int line = p->current.line;
    int col = p->current.col;
    consume(p, TOKEN_WHILE, "Expected 'while'");
    consume(p, TOKEN_LPAREN, "Expected '(' after 'while'");
    ASTExpr *cond = parse_expr(p);
    consume(p, TOKEN_RPAREN, "Expected ')' after 'while' condition");
    ASTStmt *body = parse_stmt(p);
    return ast_stmt_while(cond, body, line, col);
}

static ASTStmt *parse_for_stmt(Parser *p) {
    int line = p->current.line;
    int col = p->current.col;
    consume(p, TOKEN_FOR, "Expected 'for'");
    consume(p, TOKEN_LPAREN, "Expected '(' after 'for'");

    ASTStmt *init = NULL;
    if (match(p, TOKEN_SEMICOLON)) {
        init = NULL;
    } else if (is_type_specifier(p->current.kind)) {
        init = parse_decl_stmt(p);
    } else {
        ASTExpr *e = parse_expr(p);
        consume(p, TOKEN_SEMICOLON, "Expected ';' after for init clause");
        init = ast_stmt_expr(e, line, col);
    }

    ASTExpr *cond = NULL;
    if (!check(p, TOKEN_SEMICOLON)) {
        cond = parse_expr(p);
    }
    consume(p, TOKEN_SEMICOLON, "Expected ';' after for condition clause");

    ASTExpr *step = NULL;
    if (!check(p, TOKEN_RPAREN)) {
        step = parse_expr(p);
    }
    consume(p, TOKEN_RPAREN, "Expected ')' after for clauses");

    ASTStmt *body = parse_stmt(p);
    return ast_stmt_for(init, cond, step, body, line, col);
}

static ASTStmt *parse_return_stmt(Parser *p) {
    int line = p->current.line;
    int col = p->current.col;
    consume(p, TOKEN_RETURN, "Expected 'return'");
    ASTExpr *val = NULL;
    if (!check(p, TOKEN_SEMICOLON)) {
        val = parse_expr(p);
    }
    consume(p, TOKEN_SEMICOLON, "Expected ';' after return value");
    return ast_stmt_return(val, line, col);
}

static ASTStmt *parse_break_stmt(Parser *p) {
    int line = p->current.line;
    int col = p->current.col;
    consume(p, TOKEN_BREAK, "Expected 'break'");
    consume(p, TOKEN_SEMICOLON, "Expected ';' after 'break'");
    return ast_stmt_break(line, col);
}

static ASTStmt *parse_continue_stmt(Parser *p) {
    int line = p->current.line;
    int col = p->current.col;
    consume(p, TOKEN_CONTINUE, "Expected 'continue'");
    consume(p, TOKEN_SEMICOLON, "Expected ';' after 'continue'");
    return ast_stmt_continue(line, col);
}

static ASTStmt *parse_block(Parser *p) {
    int line = p->current.line;
    int col = p->current.col;
    consume(p, TOKEN_LBRACE, "Expected '{'");

    ASTStmt **stmts = NULL;
    int count = 0;

    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        ASTStmt *stmt = parse_stmt(p);
        count++;
        stmts = xrealloc(stmts, sizeof(ASTStmt*) * count);
        stmts[count - 1] = stmt;
    }

    consume(p, TOKEN_RBRACE, "Expected '}' at end of block");
    return ast_stmt_block(stmts, count, line, col);
}

static ASTStmt *parse_stmt(Parser *p) {
    if (check(p, TOKEN_LBRACE)) return parse_block(p);
    if (check(p, TOKEN_IF)) return parse_if_stmt(p);
    if (check(p, TOKEN_WHILE)) return parse_while_stmt(p);
    if (check(p, TOKEN_FOR)) return parse_for_stmt(p);
    if (check(p, TOKEN_RETURN)) return parse_return_stmt(p);
    if (check(p, TOKEN_BREAK)) return parse_break_stmt(p);
    if (check(p, TOKEN_CONTINUE)) return parse_continue_stmt(p);
    if (is_type_specifier(p->current.kind)) return parse_decl_stmt(p);

    int line = p->current.line;
    int col = p->current.col;
    ASTExpr *expr = parse_expr(p);
    consume(p, TOKEN_SEMICOLON, "Expected ';' after expression statement");
    return ast_stmt_expr(expr, line, col);
}

static ASTFunction *parse_function(Parser *p) {
    int line = p->current.line;
    int col = p->current.col;
    DataType ret_type = parse_type(p);
    Token name_tok = consume(p, TOKEN_IDENT, "Expected function name");

    consume(p, TOKEN_LPAREN, "Expected '(' after function name");

    ASTParam *params = NULL;
    int param_count = 0;

    if (!check(p, TOKEN_RPAREN)) {
        do {
            DataType ptype = parse_type(p);
            Token pname = consume(p, TOKEN_IDENT, "Expected parameter name");
            param_count++;
            params = xrealloc(params, sizeof(ASTParam) * param_count);
            params[param_count - 1].type = ptype;
            params[param_count - 1].name = xstrdup(pname.text);
            params[param_count - 1].line = pname.line;
            params[param_count - 1].col = pname.col;
        } while (match(p, TOKEN_COMMA));
    }

    consume(p, TOKEN_RPAREN, "Expected ')' after parameter list");

    ASTStmt *body = parse_block(p);

    ASTFunction *fn = xmalloc(sizeof(ASTFunction));
    fn->return_type = ret_type;
    fn->name = xstrdup(name_tok.text);
    fn->params = params;
    fn->param_count = param_count;
    fn->body = body;
    fn->line = line;
    fn->col = col;
    return fn;
}

ASTProgram *parse_program(Parser *parser) {
    ASTProgram *prog = ast_program_new();
    while (!check(parser, TOKEN_EOF)) {
        ASTFunction *func = parse_function(parser);
        ast_program_add_func(prog, func);
    }
    return prog;
}
