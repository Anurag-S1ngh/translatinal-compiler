#ifndef TRCO_AST_H
#define TRCO_AST_H

#include <stdbool.h>

typedef enum {
    TYPE_VOID,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_CHAR,
    TYPE_UNKNOWN
} DataType;

const char *data_type_to_string(DataType type);

typedef enum {
    EXPR_INT_LIT,
    EXPR_FLOAT_LIT,
    EXPR_CHAR_LIT,
    EXPR_STR_LIT,
    EXPR_VAR,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_ASSIGN,
    EXPR_CALL,
    EXPR_CAST
} ExprKind;

typedef enum {
    UNARY_NEG,      // -
    UNARY_NOT,      // !
    UNARY_PRE_INC,  // ++x
    UNARY_PRE_DEC,  // --x
    UNARY_POST_INC, // x++
    UNARY_POST_DEC  // x--
} UnaryOp;

typedef enum {
    BIN_ADD, BIN_SUB, BIN_MUL, BIN_DIV, BIN_MOD,
    BIN_EQ, BIN_NEQ, BIN_LT, BIN_LTE, BIN_GT, BIN_GTE,
    BIN_AND, BIN_OR
} BinaryOp;

const char *unary_op_to_string(UnaryOp op);
const char *binary_op_to_string(BinaryOp op);

typedef struct ASTExpr ASTExpr;

struct ASTExpr {
    ExprKind kind;
    DataType eval_type; // Annotated during semantic analysis
    int line;
    int col;
    union {
        long int_val;
        double float_val;
        char char_val;
        char *str_val;
        char *var_name;

        struct {
            UnaryOp op;
            ASTExpr *operand;
        } unary;

        struct {
            BinaryOp op;
            ASTExpr *left;
            ASTExpr *right;
        } binary;

        struct {
            char *var_name;
            ASTExpr *value;
        } assign;

        struct {
            char *func_name;
            ASTExpr **args;
            int arg_count;
        } call;

        struct {
            DataType target_type;
            ASTExpr *expr;
        } cast;
    };
};

typedef enum {
    STMT_DECL,
    STMT_EXPR,
    STMT_BLOCK,
    STMT_IF,
    STMT_WHILE,
    STMT_FOR,
    STMT_RETURN,
    STMT_BREAK,
    STMT_CONTINUE
} StmtKind;

typedef struct ASTStmt ASTStmt;

struct ASTStmt {
    StmtKind kind;
    int line;
    int col;
    union {
        struct {
            DataType type;
            char *var_name;
            ASTExpr *init;
        } decl;

        ASTExpr *expr;

        struct {
            ASTStmt **stmts;
            int stmt_count;
        } block;

        struct {
            ASTExpr *cond;
            ASTStmt *then_branch;
            ASTStmt *else_branch; // NULL if no else
        } if_stmt;

        struct {
            ASTExpr *cond;
            ASTStmt *body;
        } while_stmt;

        struct {
            ASTStmt *init;      // Optional decl or expr stmt
            ASTExpr *cond;      // Optional cond expr
            ASTExpr *step;      // Optional step expr
            ASTStmt *body;
        } for_stmt;

        struct {
            ASTExpr *value;     // NULL for plain return
        } return_stmt;
    };
};

typedef struct {
    DataType type;
    char *name;
    int line;
    int col;
} ASTParam;

typedef struct {
    DataType return_type;
    char *name;
    ASTParam *params;
    int param_count;
    ASTStmt *body; // STMT_BLOCK
    int line;
    int col;
} ASTFunction;

typedef struct {
    ASTFunction **functions;
    int function_count;
} ASTProgram;

// Constructors
ASTExpr *ast_expr_int_lit(long val, int line, int col);
ASTExpr *ast_expr_float_lit(double val, int line, int col);
ASTExpr *ast_expr_char_lit(char val, int line, int col);
ASTExpr *ast_expr_str_lit(const char *val, int line, int col);
ASTExpr *ast_expr_var(const char *name, int line, int col);
ASTExpr *ast_expr_unary(UnaryOp op, ASTExpr *operand, int line, int col);
ASTExpr *ast_expr_binary(BinaryOp op, ASTExpr *left, ASTExpr *right, int line, int col);
ASTExpr *ast_expr_assign(const char *var_name, ASTExpr *value, int line, int col);
ASTExpr *ast_expr_call(const char *func_name, ASTExpr **args, int arg_count, int line, int col);
ASTExpr *ast_expr_cast(DataType target_type, ASTExpr *expr, int line, int col);

ASTStmt *ast_stmt_decl(DataType type, const char *var_name, ASTExpr *init, int line, int col);
ASTStmt *ast_stmt_expr(ASTExpr *expr, int line, int col);
ASTStmt *ast_stmt_block(ASTStmt **stmts, int stmt_count, int line, int col);
ASTStmt *ast_stmt_if(ASTExpr *cond, ASTStmt *then_branch, ASTStmt *else_branch, int line, int col);
ASTStmt *ast_stmt_while(ASTExpr *cond, ASTStmt *body, int line, int col);
ASTStmt *ast_stmt_for(ASTStmt *init, ASTExpr *cond, ASTExpr *step, ASTStmt *body, int line, int col);
ASTStmt *ast_stmt_return(ASTExpr *value, int line, int col);
ASTStmt *ast_stmt_break(int line, int col);
ASTStmt *ast_stmt_continue(int line, int col);

ASTProgram *ast_program_new(void);
void ast_program_add_func(ASTProgram *prog, ASTFunction *func);

void ast_dump_program(ASTProgram *prog);
void ast_free_program(ASTProgram *prog);
void ast_expr_free(ASTExpr *expr);
void ast_stmt_free(ASTStmt *stmt);

#endif // TRCO_AST_H
