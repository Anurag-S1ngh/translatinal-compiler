#ifndef TRCO_PARSER_H
#define TRCO_PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
} Parser;

void parser_init(Parser *parser, const char *source);
ASTProgram *parse_program(Parser *parser);

#endif // TRCO_PARSER_H
