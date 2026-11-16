#ifndef TRCO_OPT_H
#define TRCO_OPT_H

#include "ast.h"

// Conservative optimization pass, run after semantic analysis.
//
// trco's subset has no pointers, no aliasing and no globals, so a local
// store whose variable is overwritten before any read is provably dead.
// The pass works on straight-line statement sequences inside blocks only:
// control-flow boundaries (if/while/for bodies) are recursed into but are
// otherwise left untouched, as is anything across a function call.
void optimize_program(ASTProgram *prog);

#endif // TRCO_OPT_H
