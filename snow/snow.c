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
#include "snow/task.h"
#include "snow/task-intern.h"
#include "snow/exception-intern.h"
#include "snow/library.h"
#include "config.h"

#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdlib.h>

static SnObject* get_closest_object(VALUE)        ATTR_HOT;

static SnClass* basic_classes[SN_TYPE_MAX];

void snow_init()
{
	snow_init_gc();
	void* base;
	GET_CALLER_BASE_PTR(base);
	snow_init_parallel(base);
	
	// base emergency exception handler
	SnContinuation main_base;
	snow_continuation_init(&main_base, NULL, NULL);
	SnTask* task = snow_get_current_task();
	task->base = &main_base;
	main_base.task_id = snow_get_current_task_id();
	if (snow_continuation_save_execution_state(&main_base)) {
		// UNHANDLED EXCEPTION ALERT!
		if (task->exception)
		{
			const char* str = snow_value_to_cstr(task->exception);
			fprintf(stderr, "UNHANDLED EXCEPTION: %s\nAborting.\n", str);
		}
		else
		{
			fprintf(stderr, "UNHANDLED UNKNOWN EXCEPTION! Aborting.\n");
		}
		exit(1);
	}
	
	// create all base classes
	SnClass* class_class;
	snow_init_class_class(&class_class);
	basic_classes[SN_CLASS_TYPE] = class_class;
	
	basic_classes[SN_OBJECT_TYPE] = snow_create_class("Object");
	basic_classes[SN_CONTINUATION_TYPE] = snow_create_class("Continuation");
	basic_classes[SN_CONTEXT_TYPE] = snow_create_class("Context");
	basic_classes[SN_ARGUMENTS_TYPE] = snow_create_class("Arguments");
	basic_classes[SN_FUNCTION_TYPE] = snow_create_class("Function");
	basic_classes[SN_FUNCTION_DESCRIPTION_TYPE] = snow_create_class("FunctionDescription");
	basic_classes[SN_STRING_TYPE] = snow_create_class("String");
	basic_classes[SN_ARRAY_TYPE] = snow_create_class("Array");
	basic_classes[SN_MAP_TYPE] = snow_create_class("Map");
	basic_classes[SN_POINTER_TYPE] = snow_create_class("Pointer");
	basic_classes[SN_EXCEPTION_TYPE] = snow_create_class("Exception");
	basic_classes[SN_CODEGEN_TYPE] = snow_create_class("Codegen");
	basic_classes[SN_AST_TYPE] = snow_create_class("AstNode");
	basic_classes[SN_INTEGER_TYPE] = snow_create_class("Integer");
	basic_classes[SN_NIL_TYPE] = snow_create_class("Nil");
	basic_classes[SN_BOOLEAN_TYPE] = snow_create_class("Boolean");
	basic_classes[SN_SYMBOL_TYPE] = snow_create_class("Symbol");
	basic_classes[SN_FLOAT_TYPE] = snow_create_class("Float");
	
	// initialize all base classes
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
	init_pointer_class(basic_classes[SN_POINTER_TYPE]);
	init_exception_class(basic_classes[SN_EXCEPTION_TYPE]);
	init_codegen_class(basic_classes[SN_CODEGEN_TYPE]);
	init_ast_class(basic_classes[SN_AST_TYPE]);
	init_integer_class(basic_classes[SN_INTEGER_TYPE]);
	init_nil_class(basic_classes[SN_NIL_TYPE]);
	init_boolean_class(basic_classes[SN_BOOLEAN_TYPE]);
	init_symbol_class(basic_classes[SN_SYMBOL_TYPE]);
	init_float_class(basic_classes[SN_FLOAT_TYPE]);
}

VALUE snow_eval(const char* str)
{
	return snow_eval_in_context(str, snow_global_context());
}

VALUE snow_eval_in_context(const char* str, SnContext* context)
{
	struct SnParserInfo info;
	SnAstNode* ast = snow_parse(str, &info);
	if (!ast) return NULL;
	SnCodegen* cg = snow_create_codegen(ast, NULL); // parent is global scope
	SnFunction* func = snow_codegen_compile(cg);
	return snow_function_call(func, context);
}

static VALUE load_paths_key = NULL;

SnArray* snow_get_load_paths()
{
	if (!load_paths_key)
	{
		SnArray* load_paths = snow_create_array();
		SnString* libdir = snow_create_string(SNOW_PATH_DATADIR "/snow/lib");
		
		snow_array_push(load_paths, libdir);
		load_paths_key = snow_store_add(load_paths);
		return load_paths;
	}
	return snow_store_get(load_paths_key);
}


static void load_source(const char* file)
{
	FILE* f = fopen(file, "r");
	SnLinkBuffer* buffer = snow_create_linkbuffer(1024);
	byte tmp[1024];
	size_t n;
	while (!feof(f))
	{
		n = fread(tmp, 1, 1024, f);
		ASSERT(n <= 1024);
		snow_linkbuffer_push_data(buffer, tmp, n);
	}
	fclose(f);
	uintx len = snow_linkbuffer_size(buffer);
	char* source = (char*)snow_malloc(len+1);
	snow_linkbuffer_copy_data(buffer, source, len);
	source[len] = '\0';
	
	const char* ps = source;
	if (ps[0] == '#' && ps[1] == '!') {
		// if there is a shebang line, ignore until first newline
		while (*ps != '\n') ++ps;
	}
	
	snow_eval(source);
	
	snow_free_linkbuffer(buffer);
	snow_free(source);
}

static void load_dynamic_library(const char* file)
{
	void* handle = dlopen(file, RTLD_NOW | RTLD_LOCAL | RTLD_FIRST);
	if (!handle)
	{
		const char* err = dlerror();
		SnString* str = snow_create_string(err);
		snow_throw_exception(str);
	}
	
	SnLibraryInfo* info = (SnLibraryInfo*)dlsym(handle, "library_info");
	
	if (!info)
	{
		snow_throw_exception_with_description("Could not find the symbol `library_info', needed to initialize Snow structures.");
	}
	debug("loading dynamic library: %s version %u\n", info->name, info->version);
	info->initialize(snow_global_context());
}

static VALUE loaded_files_key = NULL;

VALUE snow_load(const char* file)
{
	struct stat s;
	if (!stat(file, &s))
	{
		if (!loaded_files_key)		
			loaded_files_key = snow_store_add(snow_create_map_with_deep_comparison());
		SnMap* loaded_files = snow_store_get(loaded_files_key);

		SnString* file_str = snow_create_string(file);
		VALUE found = snow_map_get(loaded_files, file_str);
		if (found)
			return SN_FALSE;
		snow_map_set(loaded_files, file_str, SN_TRUE);
		// TODO: check timestamps in required_files
		
		// XXX: come up with a better way to distinguish Snow files and dynamic libraries
		uintx filename_length = strlen(file);
		bool is_snow = file[filename_length-3] == '.' && file[filename_length-2] == 's' && file[filename_length-1] == 'n';
		if (is_snow)
			load_source(file);
		else
			load_dynamic_library(file);

		return SN_TRUE;
	}
	return NULL;
}

VALUE snow_require(const char* _file)
{
	// TODO: Make this work on non-posix
	
	SnString* file = snow_create_string(_file);
	SnString* found = NULL;
	SnString* dot = snow_create_string(".");
	
	SnString* file_dot = snow_string_concatenate(file, dot);
	
	static const size_t NUM_CANDIDATES = 4;
	SnString* candidates[] = {
		file,
		snow_string_concatenate(file_dot, snow_create_string("sn")),
		snow_string_concatenate(file_dot, snow_create_string("so")),
		snow_string_concatenate(file_dot, snow_create_string("dylib")),
	};
	
	if (snow_string_byte_at(file, 0) != '/')
	{
		SnString* slash = snow_create_string("/");
		// first, check wd
		char _cwd[1024];
		getcwd(_cwd, 1024);
		SnString* cwd = snow_string_concatenate(snow_create_string(_cwd), slash);
		
		for (size_t i = 0; i < NUM_CANDIDATES; ++i)
		{
			SnString* candidate = snow_string_concatenate(cwd, candidates[i]);
			VALUE result = snow_load(snow_string_cstr(candidate));
			if (result)
				return result;
		}
		
		SnArray* load_paths = snow_get_load_paths();
		
		for (intx i = 0; i < snow_array_size(load_paths); ++i)
		{
			SnString* path = snow_string_concatenate(snow_array_get(load_paths, i), slash);
			for (size_t j = 0; j < NUM_CANDIDATES; ++j)
			{
				SnString* candidate = snow_string_concatenate(path, candidates[j]);
				VALUE result = snow_load(snow_string_cstr(candidate));
				if (result)
					return result;
			}
		}
	}
	else
	{
		VALUE result = snow_load(snow_string_cstr(file));
		if (result)
			return result;
	}
	
	snow_throw_exception_with_description("Required file not found!");
	return NULL;
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

VALUE snow_call_with_args(VALUE in_self, VALUE in_closure, SnArguments* args)
{
	VALUE self = in_self;
	VALUE closure = in_closure;
	
	if (!snow_eval_truth(closure))
	{
		snow_throw_exception_with_description("Attempted to call nil.");
	}
	
	SnSymbol call_sym = snow_symbol("__call__");
	while (snow_typeof(closure) != SN_FUNCTION_TYPE)
	{
		self = closure;
		closure = snow_get_member(closure, call_sym);
		if (!closure) snow_throw_exception_with_description("Attempted to call nil.");
	}
	
	SnFunction* func = (SnFunction*)closure;
	
	SnContext* context = snow_create_context_for_function(func);
	context->self = self;
	context->args = args;
	
	return snow_function_call(func, context);
}

VALUE snow_call_method(VALUE self, SnSymbol member, uintx num_args, ...)
{
	VALUE method = snow_get_member(self, member);
	if (!method)
		TRAP(); // no such method!
	va_list ap;
	va_start(ap, num_args);
	VALUE ret = snow_call_va(self, method, num_args, &ap);
	va_end(ap);
	return ret;
}

typedef struct ParallelThreadInvocationData {
	VALUE* results;
	SnContext* context;
} ParallelThreadInvocationData;

static void invoke_parallel_thread_function(void* _functions, size_t element_size, size_t idx, void* _userdata) {
	ParallelThreadInvocationData* userdata = (ParallelThreadInvocationData*)_userdata;
	SnFunctionDescription* desc = ((SnFunctionDescription**)_functions)[idx];
	ASSERT_TYPE(desc, SN_FUNCTION_DESCRIPTION_TYPE);
	SnFunction* func = snow_create_function_from_description(desc);
	snow_function_declared_in_context(func, userdata->context);
	SnContext* context = snow_create_context_for_function(func);
	context->self = userdata->context->self;
	context->args = userdata->context->args;
	userdata->results[idx] = snow_function_call(func, context);
}

VALUE snow_invoke_parallel_threads(SnArray* functions, SnContext* context)
{
	uintx len = snow_array_size(functions);
	VALUE results[len];
	
	ParallelThreadInvocationData userdata;
	userdata.results = results;
	userdata.context = context;
	
	snow_array_parallel_for_each(functions, invoke_parallel_thread_function, &userdata);
	
	return snow_copy_array(results, len);
}

VALUE snow_invoke_parallel_forks(SnArray* functions, SnContext* context)
{
	uintx len = snow_array_size(functions);
	VALUE pids[len];
	
	for (size_t i = 0; i < snow_array_size(functions); ++i) {
		pid_t pid = fork();
		if (pid) {
			// main process: Collect the pid in the result array, and continue.
			pids[i] = int_to_value(pid);
		} else {
			// child process: Execute the function, and then exit.
			SnFunctionDescription* desc = (SnFunctionDescription*)snow_array_get(functions, i);
			ASSERT_TYPE(desc, SN_FUNCTION_DESCRIPTION_TYPE);
			SnFunction* func = snow_create_function_from_description(desc);
			snow_function_declared_in_context(func, context);
			SnContext* ctx = snow_create_context_for_function(func);
			ctx->self = context->self;
			ctx->args = context->args;
			snow_function_call(func, ctx);
			exit(0);
		}
	}
	
	return snow_copy_array(pids, len);
}

VALUE snow_get_member(VALUE self, SnSymbol sym)
{
	SnObject* closest_object = get_closest_object(self);
	VALUE member = snow_object_get_member(closest_object, self, sym);
	return member;
}

VALUE snow_get_member_with_fallback(VALUE self, SnSymbol sym)
{
	SnObject* closest_object = get_closest_object(self);
	VALUE member = snow_object_get_member(closest_object, self, sym);
	if (!member) {
		VALUE member_missing = snow_object_get_member(closest_object, self, snow_symbol("member_missing"));
		if (member_missing) {
			member = snow_call(self, member_missing, 1, symbol_to_value(sym));
			if (member == SN_NIL) member = NULL;
		}
	}
	return member;
}

VALUE snow_get_member_for_method_call(VALUE self, SnSymbol sym)
{
	VALUE member = snow_get_member_with_fallback(self, sym);
	if (!member) snow_throw_exception_with_description("Cannot call missing member: %s on %s", snow_symbol_to_cstr(sym), snow_inspect_value(self));
	return member;
}

VALUE snow_set_member(VALUE self, SnSymbol sym, VALUE val)
{
	ASSERT(is_object(self));
	SnObject* object = (SnObject*)self;
	ASSERT(snow_is_normal_object(object));
	return snow_object_set_member(object, self, sym, val);
}

VALUE snow_get_global(SnSymbol name)
{
	SnContext* global = snow_global_context();
	return snow_context_get_local_local(global, name);
}

VALUE snow_get_global_from_context(SnSymbol name, SnContext* from)
{
	VALUE val = snow_get_global(name);
	if (!val)
	{
		val = snow_context_local_missing(from, name);
	}
	return val;
}

VALUE snow_set_global(SnSymbol name, VALUE val)
{
	SnContext* global = snow_global_context();
	return snow_context_set_local_local(global, name, val);
}

VALUE snow_set_global_from_context(SnSymbol name, VALUE val, SnContext* from)
{
	return snow_set_global(name, val);
}

VALUE snow_get_unspecified_local_from_context(SnSymbol name, SnContext* from)
{
	VALUE member = NULL;
	if (from->self) {
		member = snow_get_member(from->self, name);
		if (member) return member;
	}
	
	member = snow_get_global(name);
	if (member) return member;
	
	return snow_context_local_missing(from, name);
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
	return basic_classes[snow_typeof(self)]->instance_prototype;
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

const char* snow_value_to_cstr(VALUE val)
{
	static bool got_sym = false;
	static SnSymbol to_string;
	if (!got_sym)
	{
		to_string = snow_symbol("to_string");
		got_sym = true;
	}
	
	SnString* str = (SnString*)snow_call_method(snow_call_method(val, to_string, 0), to_string, 0);
	ASSERT_TYPE(str, SN_STRING_TYPE);
	return snow_string_cstr(str);
}

const char* snow_inspect_value(VALUE val)
{
	static bool got_sym = false;
	static SnSymbol inspect;
	if (!got_sym)
	{
		inspect = snow_symbol("inspect");
		got_sym = true;
	}
	
	SnString* str = (SnString*)snow_call_method(snow_call_method(val, inspect, 0), snow_symbol("to_string"), 0);
	ASSERT_TYPE(str, SN_STRING_TYPE);
	return snow_string_cstr(str);
}

bool snow_eval_truth(VALUE val) {
	// WARNING: This is explicitly inlined in codegens. If you change this, you must also change each codegen that inlines this.
	return !(!val || val == SN_NIL || val == SN_FALSE);
}

bool snow_is_normal_object(VALUE val) {
	if (is_object(val)) {
		SnObjectType type = snow_typeof(val);
		return type >= SN_NORMAL_OBJECT_TYPE_BASE && type < SN_NORMAL_OBJECT_TYPE_MAX;
	}
	return false;
}

bool snow_prototype_chain_contains(VALUE val, SnObject* proto) {
	SnObject* closest_object = get_closest_object(val);
	SnObject* obj = closest_object;
	while (obj != NULL) {
		ASSERT(snow_is_normal_object(obj));
		if (obj == proto) return true;
		if (snow_object_is_included(obj, proto)) return true;
		obj = obj->prototype;
	}
	
	return false;
}

int snow_compare_objects(VALUE a, VALUE b)
{
	VALUE n = snow_call_method(a, snow_symbol("<=>"), 1, b);
	ASSERT(is_integer(n));
	return value_to_int(n);
}

bool snow_is_object_assigned(VALUE val)
{
	if (snow_is_normal_object(val)) {
		SnObject* object = (SnObject*)val;
		return (object->flags & SN_FLAG_ASSIGNED) != 0;
	}
	return true;
}

VALUE snow_set_object_assigned(VALUE val, SnSymbol sym, VALUE member_of)
{
	if (!snow_is_object_assigned(val)) {
		SnObject* object = (SnObject*)val;
		object->flags |= SN_FLAG_ASSIGNED;
		snow_call_method(object, snow_symbol("__on_assign__"), 2, symbol_to_value(sym), member_of);
	}
	return val;
}

VALUE snow_reset_object_assigned(VALUE val)
{
	if (snow_is_normal_object(val)) {
		SnObject* object = (SnObject*)val;
		object->flags &= ~SN_FLAG_ASSIGNED;
	}
	return val;
}

HIDDEN SnArray** _snow_store_ptr() {
	static SnArray* array = NULL;
	if (!array)
		array = snow_create_array_with_size(1024);
	return &array;
}

VALUE snow_store_add(VALUE val) {
	SnArray* store = *_snow_store_ptr();
	intx key = snow_array_size(store);
	snow_array_push(store, val);
	return int_to_value(key);
}

VALUE snow_store_get(VALUE key) {
	ASSERT(is_integer(key));
	SnArray* store = *_snow_store_ptr();
	intx nkey = value_to_int(key);
	VALUE val = snow_array_get(store, nkey);
	return val;
}

const char* snow_version() {
	return Q(SNOW_VERSION_MAJOR) "." Q(SNOW_VERSION_MINOR) "." Q(SNOW_VERSION_BUILD) "-p" Q(SNOW_VERSION_PATCH);
}