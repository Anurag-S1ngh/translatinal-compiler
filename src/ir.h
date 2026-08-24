#ifndef TRCO_IR_H
#define TRCO_IR_H

#include "ast.h"

typedef enum {
    OPERAND_NONE,
    OPERAND_VAR,
    OPERAND_TEMP,
    OPERAND_INT_LIT,
    OPERAND_FLOAT_LIT,
    OPERAND_CHAR_LIT,
    OPERAND_STR_LIT,
    OPERAND_LABEL
} OperandKind;

typedef struct {
    OperandKind kind;
    union {
        char *name;         // VAR, TEMP, LABEL
        long int_val;       // INT_LIT
        double float_val;   // FLOAT_LIT
        char char_val;      // CHAR_LIT
        char *str_val;      // STR_LIT
    };
} IROperand;

typedef enum {
    IR_LABEL,
    IR_FUNC_START,
    IR_FUNC_END,
    IR_ASSIGN,
    IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_MOD,
    IR_NEG, IR_NOT,
    IR_EQ, IR_NEQ, IR_LT, IR_LTE, IR_GT, IR_GTE,
    IR_CAST_INT_TO_FLOAT,
    IR_CAST_FLOAT_TO_INT,
    IR_JUMP,
    IR_JUMP_IF_ZERO,
    IR_JUMP_IF_NZERO,
    IR_PARAM,
    IR_CALL,
    IR_RETURN
} IROpcode;

typedef struct {
    IROpcode op;
    IROperand arg1;
    IROperand arg2;
    IROperand result;
} IRQuad;

typedef struct {
    char *name;
    DataType return_type;
    IRQuad *quads;
    int quad_count;
    int quad_capacity;
} IRFunc;

typedef struct {
    IRFunc **funcs;
    int func_count;
} IRProgram;

// Operand constructor functions
IROperand ir_op_none(void);
IROperand ir_op_var(const char *name);
IROperand ir_op_temp(int temp_id);
IROperand ir_op_int(long val);
IROperand ir_op_float(double val);
IROperand ir_op_char(char val);
IROperand ir_op_str(const char *val);
IROperand ir_op_label(int label_id);

IRProgram *ir_generate(ASTProgram *ast);
void ir_dump_program(IRProgram *prog);
void ir_free_program(IRProgram *prog);

#endif // TRCO_IR_H
