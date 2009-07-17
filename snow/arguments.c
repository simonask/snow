#include "snow/arguments.h"
#include "snow/intern.h"
#include "snow/gc.h"

SnArguments* snow_create_arguments_with_size(uint32_t size)
{
	SnArguments* args = snow_alloc_any_object(SN_ARGUMENTS_TYPE, sizeof(SnArguments));
	args->data = size ? (VALUE*)snow_gc_alloc(size * sizeof(VALUE)) : NULL;
	args->alloc_size = size;
	args->size = 0;
	return args;
}

VALUE snow_arguments_push(SnArguments* args, VALUE val)
{
	ASSERT(args->size < args->alloc_size);
	args->data[args->size++] = val;
	return val;
}

SnObject* create_arguments_prototype()
{
	SnObject* proto = snow_create_object(NULL);
	return proto;
}