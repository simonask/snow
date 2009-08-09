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
	ctx->local_names = func->desc->local_names;
	ASSERT(ctx->local_names);
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
	
	VALUE vsym = symbol_to_value(sym);
	
	SnContext* last_ctx = NULL;
	while (ctx)
	{
		if (ctx->local_names && ctx->locals)
		{
			intx idx = snow_array_find(ctx->local_names, vsym);
			if (idx >= 0)
				return snow_array_get(ctx->locals, idx);
		}
		
		last_ctx = ctx;
		ctx = ctx->static_parent;
	}
	
	SnContext* global = snow_global_context();
	if (last_ctx != global)
	{
		// try global context, if it wasn't already in the chain.
		return snow_context_get_local(global, sym);
	}
	
	TRAP(); // no such local
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
	
	ctx = backup;
	
	if (!ctx->local_names)
	{
		ASSERT(!ctx->function); // please use snow_create_context_for_function
		ctx->local_names = snow_create_array();
	}
	
	if (!ctx->locals)
		ctx->locals = snow_create_array();
	
	intx idx = snow_array_size(ctx->local_names);
	snow_array_push(ctx->local_names, vsym);
	snow_array_set(ctx->locals, idx, val);
	
	return val;
}

void init_context_class(SnClass* klass)
{
	
}