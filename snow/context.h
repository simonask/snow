#ifndef CONTEXT_H_ZSI0OCOV
#define CONTEXT_H_ZSI0OCOV

#include "snow/basic.h"
#include "snow/array.h"
#include "snow/map.h"
#include "snow/arguments.h"

/*
	An SnContext is the context within which a function is called – it contains the
	value of `self`, the arguments, and the locals.
	
	When accessing things in parent static scopes, the parent's context object is what
	needs to be modified.
	
	If it is known that a function's context will never be accessed outside of that
	function's scope, see function.h how to call it without allocating the context.
*/

struct SnFunction;

typedef struct SnContext {
	SnObjectBase base;
	struct SnContext* static_parent;
	struct SnFunction* function;
	VALUE self;
	VALUE* locals;
	SnArguments* args;
} SnContext;


typedef VALUE(*SnFunctionPtr)(struct SnContext*);

CAPI SnContext* snow_create_context(SnContext* static_parent);
CAPI VALUE snow_context_get_named_argument(SnContext*, SnSymbol);
CAPI VALUE snow_context_get_named_argument_by_value(SnContext*, VALUE sym);
CAPI VALUE snow_context_get_local(SnContext*, SnSymbol);
CAPI VALUE snow_context_get_local_by_value(SnContext*, VALUE sym)            ATTR_HOT;

#endif /* end of include guard: CONTEXT_H_ZSI0OCOV */