#include "snow/function.h"
#include "snow/intern.h"

SnFunction* snow_create_function(SnContinuationFunc func)
{
	SnFunction* f = (SnFunction*)snow_alloc_any_object(SN_FUNCTION_TYPE, sizeof(SnFunction));
	f->func = func;
	return f;
}

VALUE snow_function_call(SnFunction* func, VALUE self, uintx num_args, VALUE* args)
{
	SnContinuation* cont = snow_create_continuation(func->func);
	snow_continuation_set_self(cont, self);
	snow_continuation_set_arguments(cont, num_args, args);
	SnContinuation* cc = snow_get_current_continuation();
	return snow_continuation_call(cont, cc);
}