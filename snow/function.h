#ifndef FUNCTION_H_CKWRVD6K
#define FUNCTION_H_CKWRVD6K

#include "snow/basic.h"
#include "snow/object.h"
#include "snow/context.h"

typedef struct SnFunction {
	SnObject base;
	SnFunctionPtr func;
	SnContext* declaration_context;
	SnArray* argument_names;
	SnMap* local_index_map;
} SnFunction;

CAPI SnFunction* snow_create_function(SnFunctionPtr func);
CAPI VALUE snow_function_call(SnFunction* func, SnContext*);

#endif /* end of include guard: FUNCTION_H_CKWRVD6K */
