#ifndef ARGUMENTS_H_4ZC67CWW
#define ARGUMENTS_H_4ZC67CWW

#include "snow/object.h"

typedef struct SnArguments {
	SnObjectBase base;
	VALUE* data;
	uint32_t alloc_size;
	uint32_t size;
	// TODO: named args
} SnArguments;

CAPI SnArguments* snow_create_arguments_with_size(uint32_t size);
CAPI VALUE snow_arguments_set_by_index(SnArguments*, uintx idx, VALUE val);
CAPI VALUE snow_arguments_push(SnArguments*, VALUE val);

#endif /* end of include guard: ARGUMENTS_H_4ZC67CWW */
