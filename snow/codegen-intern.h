#ifndef CODEGEN_INTERN_H_L1Z3N6AO
#define CODEGEN_INTERN_H_L1Z3N6AO

#include "snow/basic.h"
#include "snow/codegen.h"

HIDDEN SnCodegen* create_codegen(SnAstNode* root);
HIDDEN void codegen_free(VALUE);
HIDDEN void codegen_compile_root(SnCodegen* cgx);

#endif /* end of include guard: CODEGEN_INTERN_H_L1Z3N6AO */
