#include "snow/snow.h"
#include "snow/codegen.h"
#include "snow/arch.h"
#include "snow/str.h"
#include "snow/intern.h"
#include "snow/function.h"
#include "snow/gc.h"
#include "snow/continuation.h"
#include "snow/prototypes.h"

#include <stdarg.h>

static SnObject* find_object_prototype(VALUE)        ATTR_HOT;
static SnObject* find_immediate_prototype(VALUE)     ATTR_HOT;

static SnObject* basic_prototypes[SN_NUMBER_OF_BASIC_TYPES];

void snow_init()
{
	void* stack_top;
	GET_BASE_PTR(stack_top);
	snow_gc_stack_top(stack_top);
	
	basic_prototypes[SN_OBJECT_TYPE] = create_object_prototype();
	basic_prototypes[SN_CLASS_TYPE] = create_class_prototype();
	basic_prototypes[SN_CONTINUATION_TYPE] = create_continuation_prototype();
	basic_prototypes[SN_CONTEXT_TYPE] = create_context_prototype();
	basic_prototypes[SN_ARGUMENTS_TYPE] = create_arguments_prototype();
	basic_prototypes[SN_FUNCTION_TYPE] = create_function_prototype();
	basic_prototypes[SN_STRING_TYPE] = create_string_prototype();
	basic_prototypes[SN_ARRAY_TYPE] = create_array_prototype();
	basic_prototypes[SN_MAP_TYPE] = create_map_prototype();
	basic_prototypes[SN_WRAPPER_TYPE] = create_wrapper_prototype();
	basic_prototypes[SN_CODEGEN_TYPE] = create_codegen_prototype();
	basic_prototypes[SN_AST_TYPE] = create_ast_prototype();
	basic_prototypes[SN_INTEGER_TYPE] = create_integer_prototype();
	basic_prototypes[SN_NIL_TYPE] = create_nil_prototype();
	basic_prototypes[SN_BOOLEAN_TYPE] = create_boolean_prototype();
	basic_prototypes[SN_SYMBOL_TYPE] = create_symbol_prototype();
	basic_prototypes[SN_FLOAT_TYPE] = create_float_prototype();
	
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

VALUE snow_set_member(VALUE self, SnSymbol sym, VALUE val)
{
	// TODO: Properties
	ASSERT(is_object(self));
	SnObject* object = (SnObject*)self;
	ASSERT(object->base.type == SN_OBJECT_TYPE);
	return snow_object_set_member(object, sym, val);
}

VALUE snow_set_member_by_value(VALUE self, VALUE vsym, VALUE val)
{
	ASSERT(is_symbol(vsym));
	return snow_set_member(self, value_to_symbol(vsym), val);
}

SnObject* find_object_prototype(VALUE self)
{
	ASSERT(is_object(self));
	SnObjectBase* base = (SnObjectBase*)self;
	ASSERT(base->type < SN_NUMBER_OF_BASIC_TYPES);
	
	if (base->type == SN_OBJECT_TYPE)
		return (SnObject*)self;
	
	return basic_prototypes[base->type];
}

SnObject* find_immediate_prototype(VALUE self)
{
	ASSERT(!is_object(self));
	if (is_integer(self))
		return basic_prototypes[SN_INTEGER_TYPE];
	else if (is_float(self))
		return basic_prototypes[SN_FLOAT_TYPE];
	else if (is_nil(self))
		return basic_prototypes[SN_NIL_TYPE];
	else if (is_boolean(self))
		return basic_prototypes[SN_BOOLEAN_TYPE];
	else if (is_symbol(self))
		return basic_prototypes[SN_SYMBOL_TYPE];
	else
		TRAP();
}

SnObject** snow_get_basic_types() {
	return basic_prototypes;
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