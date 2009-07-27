#include "snow/codegen.h"
#include "snow/codegen-intern.h"

#include <sys/mman.h>

SnCodegen* snow_create_codegen(SnAstNode* root)
{
	return create_codegen(root);
}

SnFunction* snow_codegen_compile(SnCodegen* cg)
{
	return snow_create_function_from_description(snow_codegen_compile_description(cg));
}

SnFunctionDescription* snow_codegen_compile_description(SnCodegen* cg)
{
	cg->result = snow_create_function_description(NULL);
	
	codegen_compile_root(cg);
	
	uintx len = snow_linkbuffer_size(cg->buffer);
	
	// FIXME: Use an executable allocator so we can have as many functions as possible in one page of memory.
	byte* compiled_code = (byte*)valloc(len);
	snow_linkbuffer_copy_data(cg->buffer, compiled_code, len);
	mprotect(compiled_code, len, PROT_EXEC);
	
	cg->result->func = (SnFunctionPtr)compiled_code;
	
	return cg->result;
}

SnObject* create_codegen_prototype()
{
	SnObject* proto = snow_create_object(NULL);
	return proto;
}