#include "snow/context.h"
#include "snow/function.h"
#include "snow/intern.h"

SnContext* snow_create_context(SnContext* static_parent)
{
	SnContext* ctx = snow_alloc_any_object(SN_CONTEXT_TYPE, sizeof(SnContext));
	ctx->static_parent = static_parent;
	ctx->function = NULL;
	ctx->self = NULL;
	ctx->locals = NULL;
	ctx->args = NULL;
	return ctx;
}

VALUE snow_context_get_local(SnContext* ctx, SnSymbol sym)
{
	return snow_context_get_local_by_value(ctx, symbol_to_value(sym));
}

VALUE snow_context_get_local_by_value(SnContext* ctx, VALUE sym)
{
	ASSERT(is_symbol(sym));
	ASSERT(ctx->function);
	
	VALUE local_idx = NULL;
	if (ctx->function->local_index_map)
		local_idx = snow_map_get(ctx->function->local_index_map, sym);
	intx arg_idx = -1;
	if (ctx->function->argument_names)
		arg_idx = snow_array_find(ctx->function->argument_names, sym);
	
	if (local_idx)
	{
		ASSERT(is_integer(local_idx));
		int nidx = value_to_int(local_idx);
		ASSERT(nidx >= 0);
		return ctx->locals[nidx];
	}
	else if (arg_idx >= 0)
	{
		if (arg_idx < ctx->args->size)
			return ctx->args->data[arg_idx];
		else
		{
			TRAP();// arg not provided by calling method... Maybe do something clever here.
			return SN_NIL;
		}
	}
	else
	{
		// TODO: Check arguments
		if (ctx->static_parent)
			return snow_context_get_local_by_value(ctx->static_parent, sym);
		else
		{
			TRAP(); // no such local
			return NULL; // suppress warning
		}
	}
}

SnObject* create_context_prototype()
{
	SnObject* proto = snow_create_object(NULL);
	return proto;
}