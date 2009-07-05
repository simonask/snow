#include "snow/snow.h"
#include "snow/codegen.h"
#include "snow/arch.h"
#include "snow/string.h"
#include "snow/intern.h"
#include "snow/function.h"

static SnObject* find_object_prototype(VALUE);
static SnObject* find_immediate_prototype(VALUE);

void snow_init()
{
	void* stack_top;
	GET_BASE_PTR(stack_top);
	snow_gc_stack_top(stack_top);
}

VALUE snow_call_va(VALUE self, VALUE closure, uintx num_args, va_list* ap)
{
	VALUE args[num_args];
	uintx i;
	for (i = 0; i < num_args; ++i) {
		args[i] = va_arg(*ap, VALUE);
	}
	
	return snow_call_with_args(self, closure, num_args, args);
}

VALUE snow_call_with_args(VALUE self, VALUE closure, uintx num_args, VALUE* args)
{
	ASSERT(is_object(closure)); // temp
	SnFunction* func = (SnFunction*)closure;
	return snow_function_call(func, self, num_args, args);
}

VALUE snow_call_method(VALUE self, SnSymbol member, uintx num_args, ...)
{
	VALUE method = snow_get(self, member);
	va_list ap;
	va_start(ap, num_args);
	VALUE ret = snow_call_va(self, method, num_args, &ap);
	va_end(ap);
	return ret;
}

VALUE snow_get(VALUE self, SnSymbol sym)
{
	SnObject* closest_object = NULL;
	
	if (is_object(self))
		closest_object = find_object_prototype(self);
	else
		closest_object = find_immediate_prototype(self);
	
	return snow_object_get_member(closest_object, sym);
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