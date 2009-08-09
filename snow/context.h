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
	SnArray* local_names;
	SnArray* locals;
	SnArguments* args;
} SnContext;


typedef VALUE(*SnFunctionPtr)(struct SnContext*);

CAPI SnContext* snow_create_context(SnContext* static_parent);
CAPI SnContext* snow_create_context_for_function(struct SnFunction* func);
CAPI SnContext* snow_global_context();
CAPI VALUE snow_context_get_local(SnContext*, SnSymbol)                      ATTR_HOT;
CAPI VALUE snow_context_set_local(SnContext*, SnSymbol, VALUE val)           ATTR_HOT;

#endif /* end of include guard: CONTEXT_H_ZSI0OCOV */
