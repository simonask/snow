#include "snow/context.h"
#include "snow/function.h"
#include "snow/intern.h"
#include "snow/snow.h"
#include "snow/globals.h"
#include "snow/arguments.h"


SnContext* snow_create_context()
{
	SnContext* ctx = SNOW_GC_ALLOC_OBJECT(SnContext);
	ctx->function = NULL;
	ctx->self = NULL;
	ctx->locals = snow_create_array();
	ctx->args = NULL;
	return ctx;
}

void SnContext_finalize(SnContext* context) {
	// nothing
}

SnContext* snow_create_context_for_function(SnFunction* func)
{
	SnContext* ctx = SNOW_GC_ALLOC_OBJECT(SnContext);
	ctx->function = func;
	ctx->self = NULL;
	ctx->locals = snow_create_fixed_array(snow_array_size(func->desc->defined_locals) + 1); // + 1 for `it'
	ctx->args = NULL;
	return ctx;
}

static VALUE global_context_key = NULL;

SnContext* snow_global_context()
{
	if (!global_context_key)
	{
		SnContext* ctx = snow_create_context();
		global_context_key = snow_store_add(ctx);
		
		snow_init_globals(ctx);
	}
	return snow_store_get(global_context_key);
}

VALUE snow_context_get_local(SnContext* ctx, SnSymbol sym)
{	
	SnContext* top_ctx = ctx; // used for local_missing
	
	SnContext* last_ctx = NULL;
	while (ctx)
	{
		VALUE val = snow_context_get_local_local(ctx, sym);
		if (val)
			return val;
		
		last_ctx = ctx;
		ctx = ctx->function ? ctx->function->declaration_context : NULL;
	}
	
	SnContext* global = snow_global_context();
	if (last_ctx != global)
	{
		// try global context, if it wasn't already in the chain.
		return snow_context_get_local(global, sym);
	}
	
	return snow_context_local_missing(top_ctx, sym);
}

VALUE snow_context_get_local_local(SnContext* ctx, SnSymbol sym)
{
	if (ctx->function && ctx->function->desc->defined_locals && ctx->locals)
	{
		VALUE vsym = snow_symbol_to_value(sym);
		int idx = snow_array_find(ctx->function->desc->defined_locals, vsym);
		if (idx >= 0)
			return snow_array_get(ctx->locals, idx);
	}
	return NULL;
}

VALUE snow_context_set_local(SnContext* ctx, SnSymbol sym, VALUE val)
{
	VALUE vsym = snow_symbol_to_value(sym);
	
	SnContext* backup = ctx; // ctx is used below for iteration
	
	while (ctx)
	{
		if (ctx->function && ctx->function->desc->defined_locals && ctx->locals)
		{
			intx idx = snow_array_find(ctx->function->desc->defined_locals, vsym);
			if (idx >= 0)
				return snow_array_set(ctx->locals, idx, val);
		}
		
		ctx = ctx->function ? ctx->function->declaration_context : NULL;
	}
	
	return snow_context_set_local_local(backup, sym, val);
}

VALUE snow_context_set_local_local(SnContext* ctx, SnSymbol sym, VALUE val)
{
	VALUE vsym = snow_symbol_to_value(sym);
	
	if (!ctx->function)
		ctx->function = snow_create_function(NULL);
	
	if (!ctx->locals)
		ctx->locals = snow_create_array();
	
	intx idx = snow_array_find_or_add(ctx->function->desc->defined_locals, vsym);
	return snow_array_set(ctx->locals, idx, val);
}

VALUE snow_context_local_missing(SnContext* ctx, SnSymbol name)
{
	SnObject* func = (SnObject*)ctx->function;
	if (!func) func = snow_get_prototype(SnFunctionType);
	return snow_call_method(func, snow_symbol("local_missing"), 2, snow_symbol_to_value(name), ctx->self);
}

SNOW_FUNC(context_get_function) {
	ASSERT_TYPE(SELF, SnContextType);
	return ((SnContext*)SELF)->function;
}

SNOW_FUNC(context_get_arguments) {
	ASSERT_TYPE(SELF, SnContextType);
	return ((SnContext*)SELF)->args;
}

void SnContext_init_class(SnClass* klass)
{
	snow_define_property(klass, "function", context_get_function, NULL);
	snow_define_property(klass, "arguments", context_get_arguments, NULL);
}