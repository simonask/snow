#include "snow/context.h"
#include "snow/function.h"
#include "snow/intern.h"
#include "snow/snow.h"

SnContext* snow_create_context(SnContext* static_parent)
{
	SnContext* ctx = (SnContext*)snow_alloc_any_object(SN_CONTEXT_TYPE, sizeof(SnContext));
	ctx->static_parent = static_parent;
	ctx->function = NULL;
	ctx->self = NULL;
	ctx->locals = NULL;
	ctx->args = NULL;
	return ctx;
}

static VALUE global_context_key = NULL;

static VALUE global_context_func_dummy(SnContext* ctx)
{
	TRAP(); // should never be called!
}

SnContext* snow_global_context()
{
	if (!global_context_key)
	{
		SnContext* ctx = snow_create_context(NULL);
		ctx->function = snow_create_function(global_context_func_dummy);
		global_context_key = snow_store_add(ctx);
	}
	return snow_store_get(global_context_key);
}

VALUE snow_context_get_named_argument_by_value(SnContext* ctx, VALUE vsym)
{
	if (ctx->args && ctx->function && ctx->function->desc->argument_names) {
		VALUE idx = snow_array_find(ctx->function->desc->argument_names, vsym);
		return snow_arguments_get_by_index(ctx->args, value_to_int(idx));
	}
	return NULL;
}

VALUE snow_context_get_local(SnContext* ctx, SnSymbol sym)
{
	return snow_context_get_local_by_value(ctx, symbol_to_value(sym));
}

VALUE snow_context_get_local_by_value(SnContext* ctx, VALUE vsym)
{
	ASSERT(is_symbol(vsym));
	ASSERT(ctx->function);
	
	if (ctx->function->desc->local_index_map)
	{
		VALUE vidx = snow_map_get(ctx->function->desc->local_index_map, vsym);
		if (vidx)
			return snow_array_get(ctx->locals, value_to_int(vidx));
	}
	
	SnContext* parent = ctx->static_parent;
	SnContext* global = snow_global_context();
	if (!parent && ctx != global)
		parent = global;
		
	if (parent)
		return snow_context_get_local_by_value(parent, vsym);
	
	TRAP(); // no such local
	return NULL;
}

VALUE snow_context_set_local(SnContext* ctx, SnSymbol sym, VALUE val)
{
	return snow_context_set_local_by_value(ctx, symbol_to_value(sym), val);
}

static bool context_set_local_by_value_if_exists(SnContext* ctx, VALUE vsym, VALUE val)
{
	ASSERT(ctx->function);
	if (ctx->function->desc->local_index_map)
	{
		VALUE vidx = snow_map_get(ctx->function->desc->local_index_map, vsym);
		if (vidx)
		{
			snow_array_set(ctx->locals, value_to_int(vidx), val);
			return true;
		}
	}
	
	SnContext* parent = ctx->static_parent;
	SnContext* global = snow_global_context();
	if (!parent && ctx != global)
		parent = global;
		
	if (parent)
		return context_set_local_by_value_if_exists(parent, vsym, val);
	
	return false;
}

VALUE snow_context_set_local_by_value(SnContext* ctx, VALUE vsym, VALUE val)
{
	ASSERT(is_symbol(vsym));
	
	if (!context_set_local_by_value_if_exists(ctx, vsym, val))
	{
		// no existing local, add one here
		intx idx = snow_function_description_add_local(ctx->function->desc, value_to_symbol(vsym));
		if (!ctx->locals)
			ctx->locals = snow_create_array();
		snow_array_set(ctx->locals, idx, val);
	}
	
	return val;
}

SnObject* create_context_prototype()
{
	SnObject* proto = snow_create_object(NULL);
	return proto;
}