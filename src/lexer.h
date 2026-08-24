#ifndef TRCO_LEXER_H
#define TRCO_LEXER_H

#include "tokens.h"

typedef struct {
    const char *source;
    int pos;
    int line;
    int col;
} Lexer;

void lexer_init(Lexer *lexer, const char *source);
Token lexer_next_token(Lexer *lexer);
void token_free(Token *token);
void lexer_dump_tokens(const char *source);

#endif // TRCO_LEXER_H
