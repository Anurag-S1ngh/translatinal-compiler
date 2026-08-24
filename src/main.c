#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include "ir.h"

static void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <input_file> [--tokens|--ast|--ir]\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --tokens  Dump token stream from lexical analysis\n");
    fprintf(stderr, "  --ast     Dump Abstract Syntax Tree from syntax analysis\n");
    fprintf(stderr, "  --ir      Dump Three-Address Code IR (default)\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *filename = NULL;
    bool dump_tokens = false;
    bool dump_ast = false;
    bool dump_ir = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) {
            dump_tokens = true;
        } else if (strcmp(argv[i], "--ast") == 0) {
            dump_ast = true;
        } else if (strcmp(argv[i], "--ir") == 0) {
            dump_ir = true;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown flag: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            filename = argv[i];
        }
    }

    if (!filename) {
        fprintf(stderr, "Error: No input file specified.\n");
        print_usage(argv[0]);
        return 1;
    }

    if (!dump_tokens && !dump_ast && !dump_ir) {
        dump_ir = true; // Default
    }

    g_filename = filename;
    char *source = read_file(filename);

    if (dump_tokens) {
        lexer_dump_tokens(source);
        free(source);
        return g_had_error ? 1 : 0;
    }

    // Phase 1 & 2: Parse
    Parser parser;
    parser_init(&parser, source);
    ASTProgram *ast = parse_program(&parser);

    if (g_had_error) {
        free(source);
        return 1;
    }

    if (dump_ast) {
        ast_dump_program(ast);
        free(source);
        return 0;
    }

    // Phase 3: Semantic Analysis
    if (!sema_analyze_program(ast)) {
        free(source);
        return 1;
    }

    // Phase 4: IR Generation
    IRProgram *ir = ir_generate(ast);

    if (dump_ir) {
        ir_dump_program(ir);
    }

    free(source);
    return g_had_error ? 1 : 0;
}
