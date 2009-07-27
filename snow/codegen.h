#ifndef CODEGEN_H_JLFE3D9Q
#define CODEGEN_H_JLFE3D9Q

#include "snow/basic.h"
#include "snow/object.h"
#include "snow/ast.h"
#include "snow/function.h"

struct SnLinkBuffer;

typedef struct SnCodegen {
	SnObjectBase base;
	SnFunctionDescription* result;
	SnAstNode* root;
	struct SnLinkBuffer* buffer;
	// ...
} SnCodegen;

CAPI SnCodegen* snow_create_codegen(SnAstNode* root);
CAPI SnFunctionDescription* snow_codegen_compile_description(SnCodegen*);
CAPI SnFunction* snow_codegen_compile(SnCodegen*);

#endif /* end of include guard: CODEGEN_H_JLFE3D9Q */
