#include "snow/codegen.h"
#include "snow/codegen-intern.h"
#include "snow/linkbuffer.h"
#include "snow/intern.h"
#include <limits.h>
#ifndef PAGESIZE
#define PAGESIZE 4096
#endif

#include <sys/mman.h>

SnCodegen* snow_create_codegen(SnAstNode* root)
{
	return create_codegen(root);
}

SnFunction* snow_codegen_compile(SnCodegen* cg)
{
	SnFunctionDescription* desc = snow_codegen_compile_description(cg);
	return snow_create_function_from_description(desc);
}

SnFunctionDescription* snow_codegen_compile_description(SnCodegen* cg)
{
	cg->result = snow_create_function_description(NULL);
	snow_array_push(cg->result->local_names, snow_vsymbol("it"));
	
	codegen_compile_root(cg);
	
	uintx len = snow_linkbuffer_size(cg->buffer);
	
	// FIXME: Use an executable allocator so we can have as many functions as possible in one page of memory.
	// FIXME: (Snow Leopard fix) There seems to be a bug in mprotect that causes pages other than the allocated
	// ones to be marked as PROT_EXEC, and thus read-only. The following allocated 2 pages of padding on each
	// side of the allocation, to counter this overflow.
	byte* compiled_code = (byte*)snow_malloc_aligned(len+2*PAGESIZE, PAGESIZE) + PAGESIZE;
	snow_linkbuffer_copy_data(cg->buffer, compiled_code, len);
	int r = mprotect(compiled_code, len, PROT_EXEC);
	ASSERT(r == 0);
	
	cg->result->func = (SnFunctionPtr)compiled_code;
	
	return cg->result;
}

void init_codegen_class(SnClass* klass)
{
	
}