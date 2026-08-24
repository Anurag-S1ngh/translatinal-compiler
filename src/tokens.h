#ifndef TRCO_TOKENS_H
#define TRCO_TOKENS_H

typedef enum {
    TOKEN_EOF = 0,

    // Keywords
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_CHAR,
    TOKEN_VOID,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_RETURN,
    TOKEN_BREAK,
    TOKEN_CONTINUE,

    // Identifiers & Literals
    TOKEN_IDENT,
    TOKEN_INT_LIT,
    TOKEN_FLOAT_LIT,
    TOKEN_CHAR_LIT,
    TOKEN_STR_LIT,

    // Operators
    TOKEN_PLUS,         // +
    TOKEN_MINUS,        // -
    TOKEN_STAR,         // *
    TOKEN_SLASH,        // /
    TOKEN_PERCENT,      // %
    TOKEN_PLUSPLUS,     // ++
    TOKEN_MINUSMINUS,   // --
    TOKEN_ASSIGN,       // =
    TOKEN_EQ,           // ==
    TOKEN_NEQ,          // !=
    TOKEN_LT,           // <
    TOKEN_LTE,          // <=
    TOKEN_GT,           // >
    TOKEN_GTE,          // >=
    TOKEN_AND,          // &&
    TOKEN_OR,           // ||
    TOKEN_NOT,          // !

    // Punctuation
    TOKEN_LPAREN,       // (
    TOKEN_RPAREN,       // )
    TOKEN_LBRACE,       // {
    TOKEN_RBRACE,       // }
    TOKEN_SEMICOLON,    // ;
    TOKEN_COMMA,        // ,

    TOKEN_UNKNOWN
} TokenKind;

typedef struct {
    TokenKind kind;
    char *text;         // Token text representation
    int line;
    int col;
    union {
        long int_val;
        double float_val;
        char char_val;
        char *str_val;
    };
} Token;

const char *token_kind_to_string(TokenKind kind);

#endif // TRCO_TOKENS_H
