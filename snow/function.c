#include "snow/function.h"
#include "snow/continuation.h"
#include "snow/intern.h"
#include "snow/task-intern.h"
#include "snow/array-intern.h"

SnFunctionDescription* snow_create_function_description(SnFunctionPtr func)
{
	SnFunctionDescription* desc = (SnFunctionDescription*)snow_alloc_any_object(SN_FUNCTION_DESCRIPTION_TYPE, sizeof(SnFunctionDescription));
	desc->func = func;
	desc->name = snow_symbol("<unnamed>");
	desc->defined_locals = snow_create_array();
	desc->argument_names = NULL;
	desc->num_variable_reference_descriptions = 0;
	desc->variable_reference_descriptions = NULL;
	return desc;
}

CAPI uintx snow_function_description_define_local(SnFunctionDescription* desc, SnSymbol name)
{
	VALUE vsym = symbol_to_value(name);
	
	intx idx = snow_array_find(desc->defined_locals, vsym);
	if (idx < 0)
	{
		idx = snow_array_size(desc->defined_locals);
		snow_array_push(desc->defined_locals, vsym);
	}
	return (uintx)idx;
}

CAPI intx snow_function_description_get_local_index(SnFunctionDescription* desc, SnSymbol name)
{
	VALUE vsym = symbol_to_value(name);
	return snow_array_find(desc->defined_locals, vsym);
}

CAPI uintx snow_function_description_add_variable_reference(SnFunctionDescription* desc, uint32_t context_level, uint32_t variable_index)
{
	// TODO: Something more efficient than realloc... perhaps a linked list?
	uintx idx = desc->num_variable_reference_descriptions;
	desc->variable_reference_descriptions = (struct SnVariableReferenceDescription*)snow_realloc(desc->variable_reference_descriptions, (++desc->num_variable_reference_descriptions) * sizeof(struct SnVariableReferenceDescription));
	struct SnVariableReferenceDescription* varref = desc->variable_reference_descriptions + idx;
	varref->context_level = context_level;
	varref->variable_index = variable_index;
	return idx;
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
	SnFunction* func = (SnFunction*)snow_alloc_any_object(SN_FUNCTION_TYPE, sizeof(SnFunction) + sizeof(struct SnVariableReference)*desc->num_variable_reference_descriptions);
	snow_object_init((SnObject*)func, snow_get_prototype(SN_FUNCTION_TYPE));
	func->desc = desc;
	func->declaration_context = NULL;
	func->num_variable_references = desc->num_variable_reference_descriptions;
	memset(func->variable_references, 0, sizeof(struct SnVariableReference)*func->num_variable_references);
	return (SnFunction*)snow_reset_object_assigned(func);
}

static inline SnContext* context_at_level(SnContext* level1, uint32_t level)
{
	SnContext* ctx = level1;
	for (uint32_t i = 1; i < level; ++i) {
		ASSERT(ctx); // wtf? trying to get a context deeper than the static tree of contexts?
		ctx = ctx->function->declaration_context;
	}
	return ctx;
}

void snow_function_declared_in_context(SnFunction* func, SnContext* context)
{
	// This function sets up the function object after creation in a context.
	// The most important thing it does, is to set up the references to parent static scopes (contexts).
	// Unfortunately, this cannot be known before runtime, but the codegen must provide the information
	// about how to find the contexts.
	
	func->declaration_context = context;
	
	// TODO: This is terribly slow. Must be improved!
	for (uint16_t i = 0; i < func->desc->num_variable_reference_descriptions; ++i)
	{
		uint32_t level = func->desc->variable_reference_descriptions[i].context_level;
		ASSERT(level != 0); // cannot deal with references to own context, since that context doesn't exist yet.
		func->variable_references[i].context = context_at_level(context, level);
		func->variable_references[i].variable_index = func->desc->variable_reference_descriptions[i].variable_index;
	}
}

VALUE snow_function_get_referenced_variable(SnFunction* func, uint32_t idx)
{
	ASSERT(idx < func->num_variable_references);
	SnArray* ar = func->variable_references[idx].context->locals;
	VALUE var = snow_array_get(ar, func->variable_references[idx].variable_index);
	ASSERT(var != NULL);
	return var;
}

VALUE snow_function_set_referenced_variable(SnFunction* func, uint32_t idx, VALUE val)
{
	ASSERT(index < func->num_variable_references);
	SnArray* ar = func->variable_references[idx].context->locals;
	return snow_array_set(ar, func->variable_references[idx].variable_index, val);
}


static VALUE sym_it = NULL;

static void function_setup_context(SnFunction* func, SnContext* context)
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
}

VALUE snow_function_call(SnFunction* func, SnContext* context)
{
	function_setup_context(func, context);
	
	VALUE ret = func->desc->func(context);
	return ret ? ret : SN_NIL;
}

VALUE snow_function_callcc(SnFunction* func, SnContext* context)
{
	function_setup_context(func, context);
	
	SnContinuation* here = snow_get_current_continuation();
	SnContinuation base_cc;
	if (!here)
	{
		snow_continuation_init(&base_cc, NULL, NULL);
		here = &base_cc;
		snow_task_departing_from_system_stack();
	}
	
	SnContinuation* cc = snow_create_continuation(func->desc->func, context);
	VALUE ret = snow_continuation_call(cc, here);
	
	if (here == &base_cc)
	{
		snow_task_returning_to_system_stack();
	}
	
	return ret ? ret : SN_NIL;
}


SNOW_FUNC(function_local_missing) {
	REQUIRE_ARGS(2);
	ASSERT_TYPE(ARGS[1], SN_SYMBOL_TYPE);
	VALUE self = ARGS[0];
	SnSymbol sym = value_to_symbol(ARGS[1]);
	VALUE member = NULL;
	
	if (self)
	{
		member = snow_get_member(self, sym);
		if (member) return member;
	}
	
	member = snow_get_global(sym);
	if (member) return member;
	
	snow_throw_exception_with_description("Local missing: `%s'", snow_value_to_cstr(ARGS[1]));
	return NULL;
}


void init_function_class(SnClass* klass)
{
	snow_define_method(klass, "local_missing", function_local_missing);
}

void init_function_description_class(SnClass* klass)
{
	
}