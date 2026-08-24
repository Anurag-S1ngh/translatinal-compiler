#include "lexer.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

const char *token_kind_to_string(TokenKind kind) {
    switch (kind) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_INT: return "int";
        case TOKEN_FLOAT: return "float";
        case TOKEN_CHAR: return "char";
        case TOKEN_VOID: return "void";
        case TOKEN_IF: return "if";
        case TOKEN_ELSE: return "else";
        case TOKEN_WHILE: return "while";
        case TOKEN_FOR: return "for";
        case TOKEN_RETURN: return "return";
        case TOKEN_BREAK: return "break";
        case TOKEN_CONTINUE: return "continue";
        case TOKEN_IDENT: return "IDENTIFIER";
        case TOKEN_INT_LIT: return "INT_LITERAL";
        case TOKEN_FLOAT_LIT: return "FLOAT_LITERAL";
        case TOKEN_CHAR_LIT: return "CHAR_LITERAL";
        case TOKEN_STR_LIT: return "STR_LITERAL";
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_PLUSPLUS: return "++";
        case TOKEN_MINUSMINUS: return "--";
        case TOKEN_ASSIGN: return "=";
        case TOKEN_EQ: return "==";
        case TOKEN_NEQ: return "!=";
        case TOKEN_LT: return "<";
        case TOKEN_LTE: return "<=";
        case TOKEN_GT: return ">";
        case TOKEN_GTE: return ">=";
        case TOKEN_AND: return "&&";
        case TOKEN_OR: return "||";
        case TOKEN_NOT: return "!";
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_LBRACE: return "{";
        case TOKEN_RBRACE: return "}";
        case TOKEN_SEMICOLON: return ";";
        case TOKEN_COMMA: return ",";
        case TOKEN_UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN";
}

void lexer_init(Lexer *lexer, const char *source) {
    lexer->source = source;
    lexer->pos = 0;
    lexer->line = 1;
    lexer->col = 1;
}

static char peek(Lexer *lexer) {
    return lexer->source[lexer->pos];
}

static char peek_next(Lexer *lexer) {
    if (lexer->source[lexer->pos] == '\0') return '\0';
    return lexer->source[lexer->pos + 1];
}

static char advance(Lexer *lexer) {
    char c = lexer->source[lexer->pos++];
    if (c == '\n') {
        lexer->line++;
        lexer->col = 1;
    } else {
        lexer->col++;
    }
    return c;
}

static void skip_whitespace_and_comments(Lexer *lexer) {
    while (true) {
        char c = peek(lexer);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(lexer);
        } else if (c == '/' && peek_next(lexer) == '/') {
            // Line comment
            advance(lexer);
            advance(lexer);
            while (peek(lexer) != '\n' && peek(lexer) != '\0') {
                advance(lexer);
            }
        } else if (c == '/' && peek_next(lexer) == '*') {
            // Block comment
            int start_line = lexer->line;
            int start_col = lexer->col;
            advance(lexer);
            advance(lexer);
            while (peek(lexer) != '\0') {
                if (peek(lexer) == '*' && peek_next(lexer) == '/') {
                    advance(lexer);
                    advance(lexer);
                    break;
                }
                advance(lexer);
            }
            if (peek(lexer) == '\0' && (lexer->source[lexer->pos-2] != '*' || lexer->source[lexer->pos-1] != '/')) {
                error_at(start_line, start_col, "Unterminated block comment");
                break;
            }
        } else {
            break;
        }
    }
}

static Token make_token(TokenKind kind, const char *text, int line, int col) {
    Token tok;
    tok.kind = kind;
    tok.text = xstrdup(text);
    tok.line = line;
    tok.col = col;
    return tok;
}

static Token lex_number(Lexer *lexer) {
    int start_line = lexer->line;
    int start_col = lexer->col;
    StrBuf buf = strbuf_new();
    bool is_float = false;

    while (isdigit(peek(lexer))) {
        strbuf_append_char(&buf, advance(lexer));
    }

    if (peek(lexer) == '.' && isdigit(peek_next(lexer))) {
        is_float = true;
        strbuf_append_char(&buf, advance(lexer)); // '.'
        while (isdigit(peek(lexer))) {
            strbuf_append_char(&buf, advance(lexer));
        }
    }

    Token tok;
    tok.line = start_line;
    tok.col = start_col;
    tok.text = buf.data; // transfer ownership

    if (is_float) {
        tok.kind = TOKEN_FLOAT_LIT;
        tok.float_val = atof(tok.text);
    } else {
        tok.kind = TOKEN_INT_LIT;
        tok.int_val = strtol(tok.text, NULL, 10);
    }
    return tok;
}

static Token lex_identifier(Lexer *lexer) {
    int start_line = lexer->line;
    int start_col = lexer->col;
    StrBuf buf = strbuf_new();

    while (isalnum(peek(lexer)) || peek(lexer) == '_') {
        strbuf_append_char(&buf, advance(lexer));
    }

    Token tok;
    tok.line = start_line;
    tok.col = start_col;
    tok.text = buf.data;

    // Keyword lookup
    if (strcmp(tok.text, "int") == 0) tok.kind = TOKEN_INT;
    else if (strcmp(tok.text, "float") == 0) tok.kind = TOKEN_FLOAT;
    else if (strcmp(tok.text, "char") == 0) tok.kind = TOKEN_CHAR;
    else if (strcmp(tok.text, "void") == 0) tok.kind = TOKEN_VOID;
    else if (strcmp(tok.text, "if") == 0) tok.kind = TOKEN_IF;
    else if (strcmp(tok.text, "else") == 0) tok.kind = TOKEN_ELSE;
    else if (strcmp(tok.text, "while") == 0) tok.kind = TOKEN_WHILE;
    else if (strcmp(tok.text, "for") == 0) tok.kind = TOKEN_FOR;
    else if (strcmp(tok.text, "return") == 0) tok.kind = TOKEN_RETURN;
    else if (strcmp(tok.text, "break") == 0) tok.kind = TOKEN_BREAK;
    else if (strcmp(tok.text, "continue") == 0) tok.kind = TOKEN_CONTINUE;
    else tok.kind = TOKEN_IDENT;

    return tok;
}

static Token lex_char_lit(Lexer *lexer) {
    int start_line = lexer->line;
    int start_col = lexer->col;
    advance(lexer); // eat single quote

    char val = '\0';
    if (peek(lexer) == '\\') {
        advance(lexer);
        char esc = advance(lexer);
        switch (esc) {
            case 'n': val = '\n'; break;
            case 't': val = '\t'; break;
            case 'r': val = '\r'; break;
            case '0': val = '\0'; break;
            case '\\': val = '\\'; break;
            case '\'': val = '\''; break;
            default: val = esc; break;
        }
    } else {
        val = advance(lexer);
    }

    if (peek(lexer) != '\'') {
        error_at(start_line, start_col, "Unterminated character literal");
    } else {
        advance(lexer); // eat closing quote
    }

    Token tok;
    tok.kind = TOKEN_CHAR_LIT;
    tok.line = start_line;
    tok.col = start_col;
    tok.char_val = val;
    tok.text = xmalloc(4);
    snprintf(tok.text, 4, "'%c'", val >= 32 && val <= 126 ? val : '?');
    return tok;
}

static Token lex_string_lit(Lexer *lexer) {
    int start_line = lexer->line;
    int start_col = lexer->col;
    advance(lexer); // eat double quote

    StrBuf text_buf = strbuf_new();
    strbuf_append_char(&text_buf, '"');
    StrBuf val_buf = strbuf_new();

    while (peek(lexer) != '"' && peek(lexer) != '\0') {
        if (peek(lexer) == '\n') {
            error_at(start_line, start_col, "Unterminated string literal (newline in string)");
            break;
        }
        if (peek(lexer) == '\\') {
            strbuf_append_char(&text_buf, advance(lexer));
            char esc = advance(lexer);
            strbuf_append_char(&text_buf, esc);
            switch (esc) {
                case 'n': strbuf_append_char(&val_buf, '\n'); break;
                case 't': strbuf_append_char(&val_buf, '\t'); break;
                case 'r': strbuf_append_char(&val_buf, '\r'); break;
                case '0': strbuf_append_char(&val_buf, '\0'); break;
                case '\\': strbuf_append_char(&val_buf, '\\'); break;
                case '"': strbuf_append_char(&val_buf, '"'); break;
                default: strbuf_append_char(&val_buf, esc); break;
            }
        } else {
            char c = advance(lexer);
            strbuf_append_char(&text_buf, c);
            strbuf_append_char(&val_buf, c);
        }
    }

    if (peek(lexer) == '"') {
        strbuf_append_char(&text_buf, advance(lexer)); // eat closing quote
    } else {
        error_at(start_line, start_col, "Unterminated string literal");
    }

    Token tok;
    tok.kind = TOKEN_STR_LIT;
    tok.line = start_line;
    tok.col = start_col;
    tok.text = text_buf.data;
    tok.str_val = val_buf.data;
    return tok;
}

Token lexer_next_token(Lexer *lexer) {
    skip_whitespace_and_comments(lexer);

    int start_line = lexer->line;
    int start_col = lexer->col;
    char c = peek(lexer);

    if (c == '\0') {
        return make_token(TOKEN_EOF, "<EOF>", start_line, start_col);
    }

    if (isdigit(c)) {
        return lex_number(lexer);
    }

    if (isalpha(c) || c == '_') {
        return lex_identifier(lexer);
    }

    if (c == '\'') {
        return lex_char_lit(lexer);
    }

    if (c == '"') {
        return lex_string_lit(lexer);
    }

    advance(lexer);

    switch (c) {
        case '+':
            if (peek(lexer) == '+') { advance(lexer); return make_token(TOKEN_PLUSPLUS, "++", start_line, start_col); }
            return make_token(TOKEN_PLUS, "+", start_line, start_col);
        case '-':
            if (peek(lexer) == '-') { advance(lexer); return make_token(TOKEN_MINUSMINUS, "--", start_line, start_col); }
            return make_token(TOKEN_MINUS, "-", start_line, start_col);
        case '*': return make_token(TOKEN_STAR, "*", start_line, start_col);
        case '/': return make_token(TOKEN_SLASH, "/", start_line, start_col);
        case '%': return make_token(TOKEN_PERCENT, "%", start_line, start_col);
        case '=':
            if (peek(lexer) == '=') { advance(lexer); return make_token(TOKEN_EQ, "==", start_line, start_col); }
            return make_token(TOKEN_ASSIGN, "=", start_line, start_col);
        case '!':
            if (peek(lexer) == '=') { advance(lexer); return make_token(TOKEN_NEQ, "!=", start_line, start_col); }
            return make_token(TOKEN_NOT, "!", start_line, start_col);
        case '<':
            if (peek(lexer) == '=') { advance(lexer); return make_token(TOKEN_LTE, "<=", start_line, start_col); }
            return make_token(TOKEN_LT, "<", start_line, start_col);
        case '>':
            if (peek(lexer) == '=') { advance(lexer); return make_token(TOKEN_GTE, ">=", start_line, start_col); }
            return make_token(TOKEN_GT, ">", start_line, start_col);
        case '&':
            if (peek(lexer) == '&') { advance(lexer); return make_token(TOKEN_AND, "&&", start_line, start_col); }
            error_at(start_line, start_col, "Unexpected character '&' (did you mean '&&'?)");
            return make_token(TOKEN_UNKNOWN, "&", start_line, start_col);
        case '|':
            if (peek(lexer) == '|') { advance(lexer); return make_token(TOKEN_OR, "||", start_line, start_col); }
            error_at(start_line, start_col, "Unexpected character '|' (did you mean '||'?)");
            return make_token(TOKEN_UNKNOWN, "|", start_line, start_col);
        case '(': return make_token(TOKEN_LPAREN, "(", start_line, start_col);
        case ')': return make_token(TOKEN_RPAREN, ")", start_line, start_col);
        case '{': return make_token(TOKEN_LBRACE, "{", start_line, start_col);
        case '}': return make_token(TOKEN_RBRACE, "}", start_line, start_col);
        case ';': return make_token(TOKEN_SEMICOLON, ";", start_line, start_col);
        case ',': return make_token(TOKEN_COMMA, ",", start_line, start_col);
        default:
            error_at(start_line, start_col, "Unexpected character '%c'", c);
            char str[2] = {c, '\0'};
            return make_token(TOKEN_UNKNOWN, str, start_line, start_col);
    }
}

void token_free(Token *token) {
    if (token->text) {
        free(token->text);
        token->text = NULL;
    }
    if (token->kind == TOKEN_STR_LIT && token->str_val) {
        free(token->str_val);
        token->str_val = NULL;
    }
}

void lexer_dump_tokens(const char *source) {
    Lexer lexer;
    lexer_init(&lexer, source);
    printf("=== TOKENS ===\n");
    while (true) {
        Token tok = lexer_next_token(&lexer);
        printf("%3d:%-3d  %-15s '%s'\n", tok.line, tok.col, token_kind_to_string(tok.kind), tok.text);
        if (tok.kind == TOKEN_EOF) {
            token_free(&tok);
            break;
        }
        token_free(&tok);
    }
}
