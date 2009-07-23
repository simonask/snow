#ifndef FUNCTION_H_CKWRVD6K
#define FUNCTION_H_CKWRVD6K

#include "snow/basic.h"
#include "snow/object.h"
#include "snow/context.h"

typedef struct SnFunctionDescription {
	SnObjectBase base;
	SnFunctionPtr func;
	SnArray* argument_names;
	SnMap* local_index_map;
} SnFunctionDescription;

CAPI SnFunctionDescription* snow_create_function_description(SnFunctionPtr func);
CAPI intx snow_function_description_add_local(SnFunctionDescription* desc, SnSymbol sym);

typedef struct SnFunction {
	SnObject base;
	SnFunctionDescription* desc;
	SnContext* declaration_context;
} SnFunction;

CAPI SnFunction* snow_create_function(SnFunctionPtr func);
CAPI SnFunction* snow_create_function_from_description(SnFunctionDescription*);
CAPI VALUE snow_function_call(SnFunction* func, SnContext*);

#endif /* end of include guard: FUNCTION_H_CKWRVD6K */
