#include "snow/arguments.h"
#include "snow/intern.h"
#include "snow/gc.h"
#include "snow/array.h"
#include "snow/map.h"

#define NAMES (args->names)
#define DATA (args->data)

SnArguments* snow_create_arguments_with_size(uint32_t size)
{
	SnArguments* args = SNOW_GC_ALLOC_OBJECT(SnArguments);
	if (size) {
		DATA = snow_create_fixed_array(size);
		NAMES = snow_create_fixed_array(size);
	} else {
		DATA = NULL;
		NAMES = NULL;
	}
	return args;
}

void SnArguments_finalize(SnArguments* args) {
	// nothing
}

static inline void ensure_data_and_names(SnArguments* args) {
	if (!DATA || !NAMES) {
		DATA = snow_create_array();
		NAMES = snow_create_array();
	}
}

VALUE snow_arguments_push(SnArguments* args, VALUE val)
{
	ensure_data_and_names(args);
	snow_array_push(NAMES, SN_NIL);
	return snow_array_push(DATA, val);
}

VALUE snow_arguments_push_named(SnArguments* args, SnSymbol sym, VALUE val)
{
	ensure_data_and_names(args);
	intx idx = snow_array_find_or_add(NAMES, snow_symbol_to_value(sym));
	return snow_array_set(DATA, idx, val);
}

intx snow_arguments_add_name(SnArguments* args, SnSymbol sym)
{
	ensure_data_and_names(args);
	
	VALUE vsym = snow_symbol_to_value(sym);
	intx idx = snow_array_find(NAMES, vsym);
	if (idx < 0)
	{
		// not found, but first fill out the unnamed slots
		for (idx = 0; idx < snow_array_size(NAMES); ++idx)
		{
			if (!snow_eval_truth(snow_array_get(NAMES, idx)))
			{
				snow_array_set(NAMES, idx, vsym);
				return idx;
			}
		}
		
		// no unnamed slots, add it at the end
		idx = snow_array_size(NAMES);
		snow_array_push(NAMES, vsym);
	}
	return idx;
}

intx snow_arguments_get_index_for_name(SnArguments* args, SnSymbol name)
{
	if (!NAMES) return -1;
	return snow_array_find(NAMES, snow_symbol_to_value(name));
}

VALUE snow_arguments_get_by_index(SnArguments* args, intx idx)
{
	if (!DATA) return NULL;
	return snow_array_get(DATA, idx);
}

VALUE snow_arguments_get_by_name(SnArguments* args, SnSymbol name)
{
	if (!NAMES) return NULL;
	intx idx = args ? snow_array_find(NAMES, snow_symbol_to_value(name)) : -1;
	if (idx >= 0)
		return snow_array_get(DATA, idx);
	return SN_NIL;
}

VALUE snow_arguments_get_by_name_at(SnArguments* args, SnSymbol name, uintx idx)
{
	if (!DATA) return NULL;
	
	size_t unnamed_i = 0;
	VALUE unnamed = NULL;
	VALUE named = NULL;
	
	for (size_t i = 0; i < snow_array_size(DATA); ++i) {
		VALUE argname = snow_array_get(NAMES, i);
		VALUE data = snow_array_get(DATA, i);
		if (argname == SN_NIL) {
			if (unnamed_i == idx) unnamed = data;
			++unnamed_i;
		} else if (snow_value_to_symbol(argname) == name) {
			named = data;
			break;
		}
	}
	
	return named ? named : unnamed;
}

void snow_arguments_put_in_map(SnArguments* args, SnMap* map)
{
	if (!DATA) return;
	
	VALUE unnamed_args[snow_array_size(DATA)];
	uintx unnamed_size = 0;
	
	for (size_t i = 0; i < snow_array_size(NAMES); ++i) {
		VALUE argname = snow_array_get(NAMES, i);
		VALUE data = snow_array_get(DATA, i);
		if (argname == SN_NIL) {
			unnamed_args[unnamed_size++] = data;
		} else {
			snow_map_set(map, argname, data);
		}
	}
	
	if (unnamed_size % 2) snow_throw_exception_with_description("Odd number of arguments (%d) for Map.", unnamed_size);
	
	for (size_t i = 0; i < unnamed_size/2; ++i) {
		snow_map_set(map, unnamed_args[i*2], unnamed_args[i*2+1]);
	}
}

SNOW_FUNC(arguments_any) {
	ASSERT_TYPE(SELF, SnArgumentsType);
	SnArguments* args = (SnArguments*)SELF;
	return snow_boolean_to_value(DATA ? (snow_array_size(DATA) > 0) : false);
}

SNOW_FUNC(arguments_unnamed) {
	ASSERT_TYPE(SELF, SnArgumentsType);
	SnArguments* args = (SnArguments*)SELF;
	if (!DATA) return snow_create_array();
	SnArray* result = snow_create_array_with_size(snow_array_size(DATA));
	for (size_t i = 0; i < snow_array_size(DATA); ++i) {
		VALUE name = snow_array_get(NAMES, i);
		VALUE val = snow_array_get(DATA, i);
		if (name == SN_NIL) {
			snow_array_push(result, val);
		}
	}
	return result;
}

SNOW_FUNC(arguments_detect) {
	REQUIRE_ARGS(1);
	ASSERT_TYPE(SELF, SnArgumentsType);
	SnArguments* args = (SnArguments*)SELF;
	if (!DATA) return SN_NIL;
	VALUE closure = ARGS[0];
	for (size_t i = 0; i < snow_array_size(DATA); ++i) {
		VALUE x = snow_array_get(DATA, i);
		if (snow_eval_truth(snow_call(NULL, closure, 1, x))) {
			return x;
		}
	}
	return SN_NIL;
}

SNOW_FUNC(arguments_map_pairs) {
	REQUIRE_ARGS(1);
	ASSERT_TYPE(SELF, SnArgumentsType);
	SnArguments* args = (SnArguments*)SELF;
	if (!NAMES) return snow_create_array();
	VALUE closure = ARGS[0];
	SnArray* result = snow_create_array_with_size(snow_array_size(NAMES));
	for (size_t i = 0; i < snow_array_size(NAMES); ++i) {
		VALUE name = snow_array_get(NAMES, i);
		VALUE val = snow_array_get(DATA, i);
		if (name != SN_NIL) {
			VALUE x = snow_call(NULL, closure, 2, name, val);
			snow_array_push(result, x);
		}
	}
	return result;
}

SNOW_FUNC(arguments_inspect) {
	ASSERT_TYPE(SELF, SnArgumentsType);
	SnArguments* args = (SnArguments*)SELF;
	size_t len = NAMES ? snow_array_size(NAMES) : 0;
	
	SnArray* parts = snow_create_array_with_size(len * 2 + 2);
	SnString* colon = snow_create_string(":");
	SnString* comma = snow_create_string(", ");
	
	snow_array_push(parts, snow_create_string("<Arguments "));
	
	for (size_t i = 0; i < len; ++i) {
		VALUE name = snow_array_get(NAMES, i);
		VALUE value = snow_array_get(DATA, i);
		
		if (i > 0)
			snow_array_push(parts, comma);
		
		if (!snow_is_nil(name)) {
			snow_array_push(parts, name);
			snow_array_push(parts, colon);
		}
		
		snow_array_push(parts, snow_call_method(value, snow_symbol("inspect"), 0));
	}
	snow_array_push(parts, snow_create_string(">"));
	
	return snow_array_join(parts, "");
}

void SnArguments_init_class(SnClass* klass)
{
	snow_define_property(klass, "any?", arguments_any, NULL);
	snow_define_property(klass, "unnamed", arguments_unnamed, NULL);
	snow_define_method(klass, "detect", arguments_detect);
	snow_define_method(klass, "map_pairs", arguments_map_pairs);
	snow_define_method(klass, "inspect", arguments_inspect);
}
