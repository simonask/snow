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
#include "snow/linkbuffer.h"
#include "config.h"

#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

static SnObject* get_closest_object(VALUE)        ATTR_HOT;

static SnClass* basic_classes[SN_TYPE_MAX];

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
	basic_classes[SN_FUNCTION_DESCRIPTION_TYPE] = snow_create_class("FunctionDescription");
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
	init_function_description_class(basic_classes[SN_FUNCTION_DESCRIPTION_TYPE]);
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
	return snow_eval_in_context(str, snow_global_context());
}

VALUE snow_eval_in_context(const char* str, SnContext* context)
{
	SnAstNode* ast = snow_parse(str);
	SnCodegen* cg = snow_create_codegen(ast);
	SnFunction* func = snow_codegen_compile(cg);
	
	return snow_function_call(func, context);
}

static VALUE load_paths_key = NULL;

SnArray* snow_get_load_paths()
{
	if (!load_paths_key)
	{
		SnArray* load_paths = snow_create_array();
		SnString* libdir = snow_create_string(SNOW_PATH_DATADIR "/snow");
		
		snow_array_push(load_paths, libdir);
		load_paths_key = snow_store_add(load_paths);
		return load_paths;
	}
	return snow_store_get(load_paths_key);
}

static VALUE required_files_key = NULL;

VALUE snow_require(const char* _file)
{
	// TODO: Make this work on non-posix
	
	SnString* file = snow_create_string(_file);
	SnString* found = NULL;
	
	struct stat s;
	
	if (file->str[0] != '/')
	{
		SnString* slash = snow_create_string("/");
		// first, check wd
		char _cwd[1024];
		getcwd(_cwd, 1024);
		SnString* cwd = snow_string_concatenate(snow_create_string(_cwd), slash);
		
		SnString* candidate = snow_string_concatenate(cwd, file);
		if (!stat(candidate->str, &s))
		{
			// found!
			found = candidate;
		}
		else
		{
			// not found, check load paths
			SnArray* load_paths = snow_get_load_paths();
			for (intx i = 0; i < snow_array_size(load_paths); ++i)
			{
				SnString* path = snow_string_concatenate(snow_array_get(load_paths, i), slash);
				if (path->str[0] != '/')
					path = snow_string_concatenate(cwd, path);
				candidate = snow_string_concatenate(path, file);
				if (!stat(candidate->str, &s))
				{
					// found!
					found = candidate;
					break;
				}
			}
		}
	}
	
	if (!found)
		TRAP(); // file not found
	
	if (!required_files_key)		
		required_files_key = snow_store_add(snow_create_map());
	SnMap* required_files = snow_store_get(required_files_key);
	
	// TODO: check timestamps in required_files

	
	FILE* f = fopen(found->str, "r");
	SnLinkBuffer* buffer = snow_create_linkbuffer(1024);
	char tmp[1024];
	size_t n;
	while (!feof(f))
	{
		n = fread(tmp, 1, 1024, f);
		ASSERT(n < 1024);
		snow_linkbuffer_push_data(buffer, tmp, n);
	}
	fclose(f);
	uintx len = snow_linkbuffer_size(buffer);
	char* source = (char*)malloc(len+1);
	snow_linkbuffer_copy_data(buffer, source, len);
	source[len] = '\0';
	VALUE result = snow_eval(source);
	snow_free_linkbuffer(buffer);
	free(source);
	
	return SN_TRUE;
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
	STACK_GUARD;
	
	SnSymbol call_sym = snow_symbol("__call__");
	while (typeof(closure) != SN_FUNCTION_TYPE)
	{
		self = closure;
		closure = snow_get_member(closure, call_sym);
	}
	
	SnFunction* func = (SnFunction*)closure;
	ASSERT_TYPE(func, SN_FUNCTION_TYPE);
	
	// TODO: Proper context setup
	SnContext* context = snow_create_context_for_function(func);
	context->self = self;
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
	SnObject* closest_object = get_closest_object(self);
	return snow_object_get_member(closest_object, self, sym);
}

VALUE snow_set_member(VALUE self, SnSymbol sym, VALUE val)
{
	// TODO: Properties
	ASSERT(is_object(self));
	SnObject* object = (SnObject*)self;
	//ASSERT(object->base.type == SN_OBJECT_TYPE);	// TODO: A predictable way to discern if an object type is derived from SnObject or SnObjectBase directly.
	return snow_object_set_member(object, self, sym, val);
}

inline SnObject* get_closest_object(VALUE self)
{
	if (is_object(self))
	{
		SnObjectBase* base = (SnObjectBase*)self;
		uintx mask = base->type & SN_TYPE_MASK;
		switch (mask)
		{
			case SN_NORMAL_OBJECT_TYPE_BASE:
				return (SnObject*)base;
			case SN_THIN_OBJECT_TYPE_BASE:
				return snow_get_prototype(base->type);
			default:
				TRAP(); // Unknown object type ... what's going on?
		}
	}
	return basic_classes[typeof(self)]->instance_prototype;
}

SnClass* snow_get_class(SnObjectType type) {
	ASSERT(type >= 0 && type < SN_TYPE_MAX);
	return basic_classes[type];
}

SnClass** snow_get_basic_types() {
	return basic_classes;
}

SnObject* snow_get_prototype(SnObjectType type) {
	ASSERT(type >= 0 && type < SN_TYPE_MAX);
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
	
	SnString* str = (SnString*)snow_call_method(val, to_string, 0);
	ASSERT_TYPE(str, SN_STRING_TYPE);
	return str->str;
}

bool snow_eval_truth(VALUE val) {
	return !(!val || val == SN_NIL || val == SN_FALSE);
}

static SnArray** store_ptr() {
	static SnArray* array = NULL;
	if (!array)
		array = snow_create_array_with_size(1024);
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