#include "snow/context.h"
#include "snow/function.h"
#include "snow/intern.h"
#include "snow/snow.h"
#include "snow/globals.h"
#include "snow/arguments.h"


SnContext* snow_create_context(SnContext* static_parent)
{
	SnContext* ctx = (SnContext*)snow_alloc_any_object(SN_CONTEXT_TYPE, sizeof(SnContext));
	ctx->static_parent = static_parent;
	ctx->function = NULL;
	ctx->self = NULL;
	ctx->local_names = NULL;
	ctx->locals = snow_create_array();
	ctx->args = NULL;
	return ctx;
}

SnContext* snow_create_context_for_function(SnFunction* func)
{
	SnContext* ctx = (SnContext*)snow_alloc_any_object(SN_CONTEXT_TYPE, sizeof(SnContext));
	ctx->static_parent = func->declaration_context;
	ctx->function = func;
	ctx->self = NULL;
	ctx->local_names = func->desc->defined_locals;
	ctx->locals = snow_create_array_with_size(snow_array_size(ctx->local_names));
	ctx->args = NULL;
	return ctx;
}

static VALUE global_context_key = NULL;

static VALUE global_context_func_dummy(SnContext* ctx)
{
	TRAP(); // should never be called!
	return NULL;
}

SnContext* snow_global_context()
{
	if (!global_context_key)
	{
		SnContext* ctx = snow_create_context(NULL);
		global_context_key = snow_store_add(ctx);
		
		snow_init_globals(ctx);
	}
	return snow_store_get(global_context_key);
}

VALUE snow_context_get_local(SnContext* ctx, SnSymbol sym)
{
	STACK_GUARD;
	
	SnContext* top_ctx = ctx; // used for local_missing
	
	SnContext* last_ctx = NULL;
	while (ctx)
	{
		VALUE val = snow_context_get_local_local(ctx, sym);
		if (val)
			return val;
		
		last_ctx = ctx;
		ctx = ctx->static_parent;
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
	if (ctx->local_names && ctx->locals)
	{
		VALUE vsym = symbol_to_value(sym);
		int idx = snow_array_find(ctx->local_names, vsym);
		if (idx >= 0)
			return snow_array_get(ctx->locals, idx);
	}
	return NULL;
}

VALUE snow_context_set_local(SnContext* ctx, SnSymbol sym, VALUE val)
{
	VALUE vsym = symbol_to_value(sym);
	
	SnContext* backup = ctx; // ctx is used below for iteration
	
	while (ctx)
	{
		if (ctx->local_names && ctx->locals)
		{
			intx idx = snow_array_find(ctx->local_names, vsym);
			if (idx >= 0)
				return snow_array_set(ctx->locals, idx, val);
		}
		
		ctx = ctx->static_parent;
	}
	
	return snow_context_set_local_local(backup, sym, val);
}

VALUE snow_context_set_local_local(SnContext* ctx, SnSymbol sym, VALUE val)
{
	VALUE vsym = symbol_to_value(sym);
	
	if (!ctx->local_names)
	{
		ASSERT(!ctx->function); // please use snow_create_context_for_function
		ctx->local_names = snow_create_array();
	}
	
	if (!ctx->locals)
		ctx->locals = snow_create_array();
	
	intx idx = snow_array_find_or_add(ctx->local_names, vsym);
	return snow_array_set(ctx->locals, idx, val);
}

VALUE snow_context_local_missing(SnContext* ctx, SnSymbol name)
{
	SnObject* func = ctx->function;
	if (!func) func = snow_get_prototype(SN_FUNCTION_TYPE);
	return snow_call_method(func, snow_symbol("local_missing"), 2, symbol_to_value(name), ctx->self);
}

void init_context_class(SnClass* klass)
{
	
}