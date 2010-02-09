#include "snow/codegen.h"
#include "snow/codegen-intern.h"
#include "snow/linkbuffer.h"
#include "snow/intern.h"
#include "snow/array-intern.h"
#include <limits.h>
#ifndef PAGESIZE
#define PAGESIZE 4096
#endif

#include <sys/mman.h>

void codegen_init(SnCodegen* cg, SnAstNode* root, SnCodegen* parent)
{
	cg->result = NULL;
	array_init(&cg->variable_reference_names);
	cg->parent = parent;
	cg->root = root;
	cg->buffer = snow_create_linkbuffer(1024);
}

SnFunction* snow_codegen_compile(SnCodegen* cg)
{
	SnFunctionDescription* desc = snow_codegen_compile_description(cg);
	return snow_create_function_from_description(desc);
}

SnFunctionDescription* snow_codegen_compile_description(SnCodegen* cg)
{
	cg->result = snow_create_function_description(NULL);
	snow_function_description_define_local(cg->result, snow_symbol("it"));
	
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
	
	CAST_DATA_TO_FUNCTION(cg->result->func, compiled_code);
	
	cg->result->ast = cg->root;
	
	return cg->result;
}

bool codegen_variable_reference(SnCodegen* codegen, SnSymbol variable_name, uint32_t* out_reference_index)
{
	// check if we've already found the name, and reuse if so
	VALUE vsym = symbol_to_value(variable_name);
	intx idx = array_find(&codegen->variable_reference_names, vsym);
	if (idx >= 0)
	{
		*out_reference_index = idx;
		return true;
	}
	
	// check parent codegens' lists of locals, and find the index
	intx context_level = 0;
	intx variable_index = -1; // -1 at the end means not found!
	SnCodegen* cg = codegen;
	while (cg)
	{
		variable_index = snow_function_description_get_local_index(cg->result, variable_name);
		if (variable_index >= 0) break;
		cg = cg->parent;
		++context_level;
	}
	
	if (variable_index < 0)
	{
		return false;
	}
	
	// .. then add it to the list, and set the out_reference_index to the index of the index (!)
	*out_reference_index = snow_function_description_add_variable_reference(codegen->result, context_level, variable_index);
	ASSERT(array_size(&codegen->variable_reference_names) == *out_reference_index);
	array_push(&codegen->variable_reference_names, vsym);
	
	return true;
}

void init_codegen_class(SnClass* klass)
{
	
}