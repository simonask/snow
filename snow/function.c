#include "snow/function.h"
#include "snow/intern.h"

SnFunction* snow_create_native_function(SnNativeFunction func)
{
	SnFunction* f = (SnFunction*)snow_alloc_any_object(SN_NATIVE_FUNCTION_TYPE, sizeof(SnFunction));
	f->native = func;
	return f;
}

SnFunction* snow_create_snow_function(SnSnowFunction func)
{
	
}

VALUE snow_function_call(SnFunction* func, VALUE self, uintx num_args, VALUE* args)
{
	switch (func->base.type)
	{
		case SN_NATIVE_FUNCTION_TYPE:
			return func->native(self, num_args, args);
		case SN_SNOW_FUNCTION_TYPE:
			ASSERT(false);
			break;
		default:
			ASSERT(false && "Wrong type for function.");
			break;
	}
	return kNil;
}