#ifndef CODEGEN_H_JLFE3D9Q
#define CODEGEN_H_JLFE3D9Q

#include "snow/basic.h"
#include "snow/object.h"
#include "snow/ast.h"
#include "snow/function.h"

typedef struct SnCodegen {
	SnObjectBase base;
	SnAstNode* root;
	// ...
} SnCodegen;

CAPI SnCodegen* snow_create_codegen(SnAstNode* root);
CAPI SnFunction* snow_codegen_compile(SnCodegen*);

#endif /* end of include guard: CODEGEN_H_JLFE3D9Q */
