#ifndef FUNCTION_H_CKWRVD6K
#define FUNCTION_H_CKWRVD6K

#include "snow/basic.h"
#include "snow/object.h"
#include "snow/context.h"

struct SnVariableReferenceDescription {
	uint32_t context_level; // 0 is current context, 1 is parent, 2 is grandparent, etc.
	uint32_t variable_index; // 0 is first variable in context, 1 is second, etc.
};

CAPI SnFunctionDescription* snow_create_function_description(SnFunctionPtr func);
CAPI uintx snow_function_description_define_local(SnFunctionDescription*, SnSymbol name);
CAPI intx snow_function_description_get_local_index(SnFunctionDescription*, SnSymbol name);
CAPI uintx snow_function_description_add_variable_reference(SnFunctionDescription*, uint32_t context_level, uint32_t variable_index);

struct SnVariableReference {
	struct SnContext* context;
	uint16_t variable_index;
};

CAPI SnFunction* snow_create_function(SnFunctionPtr func);
CAPI SnFunction* snow_create_function_with_name(SnFunctionPtr func, const char* name);
CAPI SnFunction* snow_create_function_from_description(SnFunctionDescription*);

CAPI void snow_function_declared_in_context(SnFunction* func, SnContext*)                    ATTR_HOT;

// accessing variables in parent static scopes, by precalculated index
CAPI VALUE snow_function_get_referenced_variable(SnFunction* func, uint32_t index)           ATTR_HOT;
CAPI VALUE snow_function_set_referenced_variable(SnFunction* func, uint32_t index, VALUE)    ATTR_HOT;

// calling
CAPI VALUE snow_function_call(SnFunction* func, SnContext*)                                  ATTR_HOT;
CAPI VALUE snow_function_callcc(SnFunction* func, SnContext*);

#endif /* end of include guard: FUNCTION_H_CKWRVD6K */
