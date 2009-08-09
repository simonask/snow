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

void init_arguments_class(SnClass* klass)
{
	// TODO
}