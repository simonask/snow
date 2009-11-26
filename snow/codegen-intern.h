#ifndef CODEGEN_INTERN_H_L1Z3N6AO
#define CODEGEN_INTERN_H_L1Z3N6AO

#include "snow/basic.h"
#include "snow/codegen.h"

HIDDEN void codegen_init(SnCodegen* cg, SnAstNode* root, SnCodegen* parent);
HIDDEN void codegen_free(VALUE);
HIDDEN void codegen_compile_root(SnCodegen* cg);

// setting information for accessing statically scoped variables
HIDDEN bool codegen_variable_reference(SnCodegen* cg, SnSymbol variable_name, uint32_t* out_reference_index);

#endif /* end of include guard: CODEGEN_INTERN_H_L1Z3N6AO */
