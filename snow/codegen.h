#ifndef CODEGEN_H_JLFE3D9Q
#define CODEGEN_H_JLFE3D9Q

#include "snow/basic.h"
#include "snow/object.h"
#include "snow/ast.h"
#include "snow/function.h"

struct SnLinkBuffer;

typedef struct SnCodegen {
	SnObjectBase base;
	struct SnCodegen* parent; // The codegen that is compiling the function in which this function is defined
	SnFunctionDescription* result;
	struct array_t variable_reference_names; // map from name -> index, where index is an index in the "result"'s list of variable references
	
	SnAstNode* root;
	struct SnLinkBuffer* buffer;
	// "Derive" from this in your arch specializations.
} SnCodegen;

CAPI SnCodegen* snow_create_codegen(SnAstNode* root, SnCodegen* parent_function_codegen /* NULL means global */);
CAPI SnFunctionDescription* snow_codegen_compile_description(SnCodegen*);
CAPI SnFunction* snow_codegen_compile(SnCodegen*);

#endif /* end of include guard: CODEGEN_H_JLFE3D9Q */
