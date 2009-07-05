#ifndef FUNCTION_H_CKWRVD6K
#define FUNCTION_H_CKWRVD6K

#include "snow/basic.h"
#include "snow/object.h"

typedef VALUE(*SnNativeFunction)(VALUE self, uintx num_args, VALUE* args);
typedef VALUE(*SnSnowFunction)(void* scope);

typedef struct SnFunction {
	SnObject base;
	union {
		SnNativeFunction native;
		SnSnowFunction ptr;
	};
} SnFunction;

CAPI SnFunction* snow_create_native_function(SnNativeFunction func);
CAPI SnFunction* snow_create_snow_function(SnSnowFunction func);
CAPI VALUE snow_function_call(SnFunction* func, VALUE self, uintx num_args, VALUE* args);

#endif /* end of include guard: FUNCTION_H_CKWRVD6K */
