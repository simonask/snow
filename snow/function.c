#include "snow/function.h"
#include "snow/continuation.h"
#include "snow/intern.h"

SnFunctionDescription* snow_create_function_description(SnFunctionPtr func)
{
	SnFunctionDescription* desc = (SnFunctionDescription*)snow_alloc_any_object(SN_FUNCTION_DESCRIPTION_TYPE, sizeof(SnFunctionDescription));
	snow_object_init((SnObject*)desc, snow_get_prototype(SN_FUNCTION_TYPE));
	desc->func = func;
	desc->name = snow_symbol("<unnamed>");
	desc->argument_names = NULL;
	desc->local_index_map = NULL;
	return desc;
}

intx snow_function_description_add_local(SnFunctionDescription* desc, SnSymbol sym)
{
	if (!desc->local_index_map)
		desc->local_index_map = snow_create_map();
	VALUE vsym = symbol_to_value(sym);
	VALUE existing = snow_map_get(desc->local_index_map, vsym);
	if (existing)
		return value_to_int(existing);
	
	// not found, insert
	intx idx = snow_map_size(desc->local_index_map);
	snow_map_set(desc->local_index_map, symbol_to_value(sym), int_to_value(idx));
	return idx;
}

SnFunction* snow_create_function(SnFunctionPtr func)
{
	return snow_create_function_from_description(snow_create_function_description(func));
}

SnFunction* snow_create_function_with_name(SnFunctionPtr func, const char* name)
{
	SnSymbol sym = snow_symbol(name);
	SnFunctionDescription* desc = snow_create_function_description(func);
	desc->name = sym;
	return snow_create_function_from_description(desc);
}

SnFunction* snow_create_function_from_description(SnFunctionDescription* desc)
{
	ASSERT(desc);
	ASSERT(desc->base.type == SN_FUNCTION_DESCRIPTION_TYPE);
	ASSERT(desc->func);
	SnFunction* func = (SnFunction*)snow_alloc_any_object(SN_FUNCTION_TYPE, sizeof(SnFunction));
	func->desc = desc;
	func->declaration_context = NULL;
	return func;
}

VALUE snow_function_call(SnFunction* func, SnContext* context)
{
	context->function = func;
	SnContinuation* continuation = snow_create_continuation(func->desc->func, context);
	SnContinuation* here = snow_get_current_continuation();
	
	return snow_continuation_call(continuation, here);
}

void init_function_class(SnClass* klass)
{
	
}