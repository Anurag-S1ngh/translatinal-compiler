#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include "ir.h"
#include "codegen.h"

static void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <input_file.c|-> [--tokens|--ast|--ir|--translate|--emit-c|--c] [-o <output.c>]\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --tokens    Dump token stream from lexical analysis\n");
    fprintf(stderr, "  --ast       Dump Abstract Syntax Tree from syntax analysis\n");
    fprintf(stderr, "  --ir        Dump Three-Address Code IR\n");
    fprintf(stderr, "  --translate, --emit-c, --c\n");
    fprintf(stderr, "              Emit translated (re-compilable) C source\n");
    fprintf(stderr, "  -o <file>   Write translated C to <file> instead of stdout (requires --translate/--emit-c/--c)\n");
    fprintf(stderr, "\nUse '-' as the input file to read the source from standard input.\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *filename = NULL;
    const char *output_path = NULL;
    bool dump_tokens = false;
    bool dump_ast = false;
    bool dump_ir = false;
    bool dump_c = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) {
            dump_tokens = true;
        } else if (strcmp(argv[i], "--ast") == 0) {
            dump_ast = true;
        } else if (strcmp(argv[i], "--ir") == 0) {
            dump_ir = true;
        } else if (strcmp(argv[i], "--translate") == 0 ||
                   strcmp(argv[i], "--emit-c") == 0 ||
                   strcmp(argv[i], "--c") == 0) {
            dump_c = true;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -o requires an output file argument.\n");
                print_usage(argv[0]);
                return 1;
            }
            output_path = argv[++i];
        } else if (strcmp(argv[i], "-") == 0) {
            filename = argv[i];
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

    if (!dump_tokens && !dump_ast && !dump_ir && !dump_c) {
        dump_ir = true; // Default
    }

    if (output_path && !dump_c) {
        fprintf(stderr, "Error: -o requires --translate/--emit-c.\n");
        print_usage(argv[0]);
        return 1;
    }

    g_filename = (strcmp(filename, "-") == 0) ? "<stdin>" : filename;
    char *source = (strcmp(filename, "-") == 0) ? read_stdin() : read_file(filename);

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
        ast_free_program(ast);
        free(source);
        return 1;
    }

    if (dump_ast) {
        ast_dump_program(ast);
        ast_free_program(ast);
        free(source);
        return 0;
    }

    // Phase 3: Semantic Analysis
    if (!sema_analyze_program(ast)) {
        ast_free_program(ast);
        free(source);
        return 1;
    }

    // Phase 4: IR Generation
    IRProgram *ir = NULL;
    if (dump_ir) {
        ir = ir_generate(ast);
        ir_dump_program(ir);
    }

    // Phase 5: C Translation (source-to-source, semantics-preserving)
    int result = 0;
    if (dump_c) {
        if (output_path) {
            if (codegen_emit_to_file(ast, output_path) != 0) {
                fprintf(stderr, "Error: could not write to '%s'.\n", output_path);
                result = 1;
            }
        } else {
            codegen_emit_program(ast, stdout);
        }
    }

    ir_free_program(ir);
    ast_free_program(ast);
    free(source);
    if (result != 0) {
        return result;
    }
    return g_had_error ? 1 : 0;
}
