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

void snow_arguments_add_name(SnArguments* args, SnSymbol sym)
{
	VALUE vsym = symbol_to_value(sym);
	intx idx = array_find(NAMES, vsym);
	if (idx < 0)
	{
		for (idx = 0; idx < array_size(NAMES); ++idx)
		{
			if (array_get(NAMES, idx) == SN_NIL)
			{
				array_set(NAMES, idx, vsym);
				return;
			}
		}
	}
}

intx snow_arguments_get_index_for_name(SnArguments* args, SnSymbol name)
{
	return array_find(NAMES, symbol_to_value(name));
}

VALUE snow_arguments_get_by_index(SnArguments* args, intx idx)
{
	return array_get(DATA, idx);
}

void init_arguments_class(SnClass* klass)
{
	// TODO
}