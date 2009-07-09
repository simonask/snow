#ifndef FUNCTION_H_CKWRVD6K
#define FUNCTION_H_CKWRVD6K

#include "snow/basic.h"
#include "snow/object.h"
#include "snow/continuation.h"

typedef struct SnFunction {
	SnObject base;
	SnContinuationFunc func;
} SnFunction;

CAPI SnFunction* snow_create_function(SnContinuationFunc func);
CAPI VALUE snow_function_call(SnFunction* func, VALUE self, uintx num_args, VALUE* args);

#endif /* end of include guard: FUNCTION_H_CKWRVD6K */
