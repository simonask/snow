#include "snow/function.h"
#include "snow/continuation.h"
#include "snow/intern.h"

SnFunction* snow_create_function(SnFunctionPtr func)
{
	SnFunction* f = (SnFunction*)snow_alloc_any_object(SN_FUNCTION_TYPE, sizeof(SnFunction));
	f->func = func;
	f->declaration_context = NULL;
	f->argument_names = NULL;
	f->local_index_map = NULL;
	return f;
}

VALUE snow_function_call(SnFunction* func, SnContext* context)
{
	context->function = func;
	SnContinuation* continuation = snow_create_continuation(func->func, context);
	SnContinuation* here = snow_get_current_continuation();
	
	return snow_continuation_call(continuation, here);
}