#include "snow/snow.h"
#include "snow/codegen.h"
#include "snow/arch.h"
#include "snow/str.h"
#include "snow/intern.h"
#include "snow/function.h"
#include "snow/gc.h"
#include "snow/continuation.h"

#include <stdarg.h>

static SnObject* find_object_prototype(VALUE);
static SnObject* find_immediate_prototype(VALUE);

void snow_init()
{
	void* stack_top;
	GET_BASE_PTR(stack_top);
	snow_gc_stack_top(stack_top);
	snow_init_current_continuation();
}

VALUE snow_call(VALUE self, VALUE closure, uintx num_args, ...)
{
	va_list ap;
	va_start(ap, num_args);
	VALUE ret = snow_call_va(self, closure, num_args, &ap);
	va_end(ap);
	return ret;
}

VALUE snow_call_va(VALUE self, VALUE closure, uintx num_args, va_list* ap)
{
	SnArguments* args = snow_create_arguments_with_size(num_args);
	
	for (uintx i = 0; i < num_args; ++i) {
		snow_arguments_push(args, va_arg(*ap, VALUE));
	}
	
	return snow_call_with_args(self, closure, args);
}

VALUE snow_call_with_args(VALUE self, VALUE closure, SnArguments* args)
{
	ASSERT(is_object(closure)); // temp
	SnFunction* func = (SnFunction*)closure;
	ASSERT(func->base.base.type == SN_FUNCTION_TYPE);
	
	// TODO: Proper context setup
	SnContext* context = snow_create_context(func->declaration_context);
	context->self = self;
	context->locals = NULL;
	context->args = args;
	
	return snow_function_call(func, context);
}

VALUE snow_call_method(VALUE self, SnSymbol member, uintx num_args, ...)
{
	VALUE method = snow_get_member(self, member);
	va_list ap;
	va_start(ap, num_args);
	VALUE ret = snow_call_va(self, method, num_args, &ap);
	va_end(ap);
	return ret;
}

VALUE snow_get_member(VALUE self, SnSymbol sym)
{
	SnObject* closest_object = NULL;
	
	if (is_object(self))
		closest_object = find_object_prototype(self);
	else
		closest_object = find_immediate_prototype(self);
	
	return snow_object_get_member(closest_object, sym);
}

VALUE snow_get_member_by_value(VALUE self, VALUE sym)
{
	ASSERT(is_symbol(sym));
	return snow_get_member(self, value_to_symbol(sym));
}

SnObject* find_object_prototype(VALUE self)
{
	ASSERT(is_object(self));
	SnObjectBase* base = (SnObjectBase*)self;
	switch (base->type) {
		default:
		return self;
	}
	ASSERT(false);
}

SnObject* find_immediate_prototype(VALUE self)
{
	ASSERT(false);
}

const char* snow_value_to_string(VALUE val)
{
	static bool got_sym = false;
	static SnSymbol to_string;
	if (!got_sym)
	{
		to_string = snow_symbol("to_string");
		got_sym = true;
	}
	
	VALUE str_val = snow_call_method(val, to_string, 0);
	ASSERT(is_object(str_val));
	SnString* str = (SnString*)str_val;
	ASSERT(str->base.type == SN_STRING_TYPE);
	return str->str;
}