#include "snow/function.h"
#include "snow/continuation.h"
#include "snow/intern.h"

SnFunctionDescription* snow_create_function_description(SnFunctionPtr func)
{
	SnFunctionDescription* desc = (SnFunctionDescription*)snow_alloc_any_object(SN_FUNCTION_DESCRIPTION_TYPE, sizeof(SnFunctionDescription));
	desc->func = func;
	desc->name = snow_symbol("<unnamed>");
	desc->argument_names = NULL;
	desc->local_names = snow_create_array();
	desc->interruptible = false;
	return desc;
}

intx snow_function_description_add_local(SnFunctionDescription* desc, SnSymbol sym)
{
	ASSERT(desc->local_names);
	
	return snow_array_find_or_add(desc->local_names, symbol_to_value(sym));
}

SnFunction* snow_create_function(SnFunctionPtr func)
{
	return snow_create_function_from_description(snow_create_function_description(func));
}

SnFunction* snow_create_function_with_name(SnFunctionPtr func, const char* name)
{
	SnSymbol sym = snow_symbol(name);
	SnFunctionDescription* desc = snow_create_function_description(func);
	desc->name = sym;
	return snow_create_function_from_description(desc);
}

SnFunction* snow_create_function_from_description(SnFunctionDescription* desc)
{
	ASSERT(desc);
	ASSERT_TYPE(desc, SN_FUNCTION_DESCRIPTION_TYPE);
	ASSERT(desc->func);
	SnFunction* func = (SnFunction*)snow_alloc_any_object(SN_FUNCTION_TYPE, sizeof(SnFunction));
	snow_object_init((SnObject*)func, snow_get_prototype(SN_FUNCTION_TYPE));
	func->desc = desc;
	func->declaration_context = NULL;
	return func;
}

static VALUE sym_it = NULL;

VALUE snow_function_call(SnFunction* func, SnContext* context)
{
	ASSERT(context);
	
	if (!sym_it)
		sym_it = snow_vsymbol("it");
	
	VALUE it = context->args ? snow_arguments_get_by_index(context->args, 0) : NULL;
	snow_context_set_local_local(context, value_to_symbol(sym_it), it ? it : SN_NIL);
	
	// TODO: Optimize this
	SnArray* arg_names = func->desc->argument_names;
	if (arg_names)
	{
		for (intx i = 0; i < snow_array_size(arg_names); ++i)
		{
			VALUE vsym = snow_array_get(arg_names, i);
			ASSERT(is_symbol(vsym));
			SnSymbol sym = value_to_symbol(vsym);
			intx idx = snow_arguments_add_name(context->args, sym);
			VALUE arg = snow_arguments_get_by_index(context->args, idx);
			snow_context_set_local_local(context, sym, arg ? arg : SN_NIL);
		}
	}
	
	SnContinuation* here = snow_get_current_continuation();
	VALUE ret = NULL;
	if (here != NULL && !func->desc->interruptible)
	{
		// call without continuation, but set proper flags on current continuation
		bool old_interruptible = here->interruptible;
		here->interruptible = false;
		ret = func->desc->func(context);
		here->interruptible = old_interruptible;
	}
	else if (here == NULL)
	{
		// call without continuation, and there is no current continuation, so screw it
		ret = func->desc->func(context);
	}
	else
	{
		// call with continuation
		SnContinuation* continuation = snow_create_continuation(func->desc->func, context);
		ret = snow_continuation_call(continuation, here);
	}

	if (!ret)
		ret = SN_NIL;
	return ret;
}


SNOW_FUNC(function_local_missing) {
	REQUIRE_ARGS(2);
	ASSERT_TYPE(ARGS[1], SN_SYMBOL_TYPE);
	VALUE self = ARGS[0];
	SnSymbol sym = value_to_symbol(ARGS[1]);
	if (ARGS[0])
	{
		VALUE member = snow_get_member(self, sym);
		if (member) return member;
	}
	
	snow_throw_exception_with_description("Local missing: `%s'", snow_value_to_string(ARGS[1]));
	return SN_NIL;
}


void init_function_class(SnClass* klass)
{
	snow_define_method(klass, "local_missing", function_local_missing);
}

void init_function_description_class(SnClass* klass)
{
	
}