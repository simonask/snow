#include "snow/snow.h"
#include "snow/codegen.h"
#include "snow/arch.h"
#include "snow/str.h"
#include "snow/intern.h"
#include "snow/function.h"
#include "snow/gc.h"
#include "snow/continuation.h"
#include "snow/classes.h"
#include "snow/parser.h"
#include "snow/debug.h"

#include <stdarg.h>

static SnObject* find_prototype(VALUE)        ATTR_HOT;

static SnClass* basic_classes[SN_NUMBER_OF_BASIC_TYPES];

void snow_init()
{
	void* stack_top;
	GET_BASE_PTR(stack_top);
	snow_gc_stack_top(stack_top);
	
	basic_classes[SN_OBJECT_TYPE] = snow_create_class("Object");
	basic_classes[SN_CLASS_TYPE] = snow_create_class("Class");
	basic_classes[SN_CONTINUATION_TYPE] = snow_create_class("Continuation");
	basic_classes[SN_CONTEXT_TYPE] = snow_create_class("Context");
	basic_classes[SN_ARGUMENTS_TYPE] = snow_create_class("Arguments");
	basic_classes[SN_FUNCTION_TYPE] = snow_create_class("Function");
	basic_classes[SN_STRING_TYPE] = snow_create_class("String");
	basic_classes[SN_ARRAY_TYPE] = snow_create_class("Array");
	basic_classes[SN_MAP_TYPE] = snow_create_class("Map");
	basic_classes[SN_WRAPPER_TYPE] = snow_create_class("Wrapper");
	basic_classes[SN_CODEGEN_TYPE] = snow_create_class("Codegen");
	basic_classes[SN_AST_TYPE] = snow_create_class("AstNode");
	basic_classes[SN_INTEGER_TYPE] = snow_create_class("Integer");
	basic_classes[SN_NIL_TYPE] = snow_create_class("Nil");
	basic_classes[SN_BOOLEAN_TYPE] = snow_create_class("Boolean");
	basic_classes[SN_SYMBOL_TYPE] = snow_create_class("Symbol");
	basic_classes[SN_FLOAT_TYPE] = snow_create_class("Float");
	
	
	init_object_class(basic_classes[SN_OBJECT_TYPE]);
	init_class_class(basic_classes[SN_CLASS_TYPE]);
	init_continuation_class(basic_classes[SN_CONTINUATION_TYPE]);
	init_context_class(basic_classes[SN_CONTEXT_TYPE]);
	init_arguments_class(basic_classes[SN_ARGUMENTS_TYPE]);
	init_function_class(basic_classes[SN_FUNCTION_TYPE]);
	init_string_class(basic_classes[SN_STRING_TYPE]);
	init_array_class(basic_classes[SN_ARRAY_TYPE]);
	init_map_class(basic_classes[SN_MAP_TYPE]);
	init_wrapper_class(basic_classes[SN_WRAPPER_TYPE]);
	init_codegen_class(basic_classes[SN_CODEGEN_TYPE]);
	init_ast_class(basic_classes[SN_AST_TYPE]);
	init_integer_class(basic_classes[SN_INTEGER_TYPE]);
	init_nil_class(basic_classes[SN_NIL_TYPE]);
	init_boolean_class(basic_classes[SN_BOOLEAN_TYPE]);
	init_symbol_class(basic_classes[SN_SYMBOL_TYPE]);
	init_float_class(basic_classes[SN_FLOAT_TYPE]);
	
	snow_init_current_continuation();
}

VALUE snow_eval(const char* str)
{
	SnAstNode* ast = snow_parse(str);
	SnCodegen* cg = snow_create_codegen(ast);
	SnFunction* func = snow_codegen_compile(cg);
	
	// TODO: Global scope
	return snow_call(NULL, func, 0);
}

VALUE snow_require(const char* file)
{
	TRAP(); // TODO
	return SN_NIL;
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
	SnSymbol call_sym = snow_symbol("__call__");
	while (typeof(closure) != SN_FUNCTION_TYPE)
	{
		closure = snow_get_member(closure, call_sym);
	}
	
	SnFunction* func = (SnFunction*)closure;
	ASSERT_TYPE(func, SN_FUNCTION_TYPE);
	
	// TODO: Proper context setup
	SnContext* context = snow_create_context(func->declaration_context);
	context->self = self;
	context->function = func;
	context->locals = NULL;
	if (func->desc->local_index_map)
		context->locals = snow_create_array_with_size(snow_map_size(func->desc->local_index_map));
	context->args = args ? args : snow_create_arguments_with_size(0);
	
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
	SnObject* closest_object = find_prototype(self);
	
	VALUE member = snow_object_get_member(closest_object, sym);
	if (!member)
	{
//		debug("member `%s` not found on 0x%llx\n", snow_symbol_to_string(sym), self);
		TRAP(); // member not found
	}
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
	//ASSERT(object->base.type == SN_OBJECT_TYPE);	// TODO: A predictable way to discern if an object type is derived from SnObject or SnObjectBase directly.
	return snow_object_set_member(object, sym, val);
}

VALUE snow_set_member_by_value(VALUE self, VALUE vsym, VALUE val)
{
	ASSERT(is_symbol(vsym));
	return snow_set_member(self, value_to_symbol(vsym), val);
}

SnObject* find_prototype(VALUE self)
{
	if (is_object(self))
		return self;
	return basic_classes[typeof(self)]->instance_prototype;
}

SnClass* snow_get_class(SnObjectType type) {
	ASSERT(type >= 0 && type < SN_NUMBER_OF_BASIC_TYPES);
	return basic_classes[type];
}

SnClass** snow_get_basic_types() {
	return basic_classes;
}

SnObject* snow_get_prototype(SnObjectType type) {
	ASSERT(type >= 0 && type < SN_NUMBER_OF_BASIC_TYPES);
	return basic_classes[type]->instance_prototype;
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

bool snow_eval_truth(VALUE val) {
	return !(!val || val == SN_NIL || val == SN_FALSE);
}

static SnArray** store_ptr() {
	static SnArray* array = NULL;
	if (!array)
		array = snow_create_array();
	return &array;
}

VALUE snow_store_add(VALUE val) {
	SnArray* store = *store_ptr();
	intx key = snow_array_size(store);
	snow_array_push(store, val);
	return int_to_value(key);
}

VALUE snow_store_get(VALUE key) {
	ASSERT(is_integer(key));
	SnArray* store = *store_ptr();
	intx nkey = value_to_int(key);
	return snow_array_get(store, nkey);
}