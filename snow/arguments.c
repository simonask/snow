#include "snow/arguments.h"
#include "snow/intern.h"
#include "snow/gc.h"
#include "snow/array-intern.h"

#define NAMES (&args->names)
#define DATA (&args->data)

SnArguments* snow_create_arguments_with_size(uint32_t size)
{
	SnArguments* args = (SnArguments*)snow_alloc_any_object(SN_ARGUMENTS_TYPE, sizeof(SnArguments));
	array_init_with_size(DATA, size);
	array_init_with_size(NAMES, size);
	return args;
}

VALUE snow_arguments_push(SnArguments* args, VALUE val)
{
	array_push(NAMES, SN_NIL);
	return array_push(DATA, val);
}

VALUE snow_arguments_push_named(SnArguments* args, SnSymbol sym, VALUE val)
{
	intx idx = array_find_or_add(NAMES, symbol_to_value(sym));
	return array_set(DATA, idx, val);
}

intx snow_arguments_add_name(SnArguments* args, SnSymbol sym)
{
	VALUE vsym = symbol_to_value(sym);
	intx idx = array_find(NAMES, vsym);
	if (idx < 0)
	{
		// not found, but first fill out the unnamed slots
		for (idx = 0; idx < array_size(NAMES); ++idx)
		{
			if (!snow_eval_truth(array_get(NAMES, idx)))
			{
				array_set(NAMES, idx, vsym);
				return idx;
			}
		}
		
		// no unnamed slots, add it at the end
		idx = array_size(NAMES);
		array_push(NAMES, vsym);
	}
	return idx;
}

intx snow_arguments_get_index_for_name(SnArguments* args, SnSymbol name)
{
	return array_find(NAMES, symbol_to_value(name));
}

VALUE snow_arguments_get_by_index(SnArguments* args, intx idx)
{
	return array_get(DATA, idx);
}

VALUE snow_arguments_get_by_name(SnArguments* args, SnSymbol name)
{
	intx idx = args ? array_find(NAMES, symbol_to_value(name)) : -1;
	if (idx >= 0)
		return array_get(DATA, idx);
	return SN_NIL;
}

SNOW_FUNC(arguments_any) {
	ASSERT_TYPE(SELF, SN_ARGUMENTS_TYPE);
	SnArguments* args = (SnArguments*)SELF;
	return boolean_to_value(array_size(DATA) > 0);
}

SNOW_FUNC(arguments_unnamed) {
	ASSERT_TYPE(SELF, SN_ARGUMENTS_TYPE);
	SnArguments* args = (SnArguments*)SELF;
	SnArray* result = snow_create_array_with_size(array_size(DATA));
	for (size_t i = 0; i < array_size(DATA); ++i) {
		VALUE name = array_get(NAMES, i);
		VALUE val = array_get(DATA, i);
		if (name == SN_NIL) {
			snow_array_push(result, val);
		}
	}
	return result;
}

SNOW_FUNC(arguments_detect) {
	REQUIRE_ARGS(1);
	ASSERT_TYPE(SELF, SN_ARGUMENTS_TYPE);
	SnArguments* args = (SnArguments*)SELF;
	VALUE closure = ARGS[0];
	for (size_t i = 0; i < array_size(DATA); ++i) {
		VALUE x = array_get(DATA, i);
		if (snow_eval_truth(snow_call(NULL, closure, 1, x))) {
			return x;
		}
	}
	return SN_NIL;
}

SNOW_FUNC(arguments_map_pairs) {
	REQUIRE_ARGS(1);
	ASSERT_TYPE(SELF, SN_ARGUMENTS_TYPE);
	SnArguments* args = (SnArguments*)SELF;
	VALUE closure = ARGS[0];
	SnArray* result = snow_create_array_with_size(array_size(NAMES));
	for (size_t i = 0; i < array_size(NAMES); ++i) {
		VALUE name = array_get(NAMES, i);
		VALUE val = array_get(DATA, i);
		if (name != SN_NIL) {
			VALUE x = snow_call(NULL, closure, 2, name, val);
			snow_array_push(result, x);
		}
	}
	return result;
}

void init_arguments_class(SnClass* klass)
{
	snow_define_property(klass, "any?", arguments_any, NULL);
	snow_define_property(klass, "unnamed", arguments_unnamed, NULL);
	snow_define_method(klass, "detect", arguments_detect);
	snow_define_method(klass, "map_pairs", arguments_map_pairs);
}
