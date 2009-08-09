#ifndef ARGUMENTS_H_4ZC67CWW
#define ARGUMENTS_H_4ZC67CWW

#include "snow/object.h"
#include "snow/map.h"

typedef struct SnArguments {
	SnObjectBase base;
	struct array_t names;
	struct array_t data;
} SnArguments;

CAPI SnArguments* snow_create_arguments_with_size(uint32_t size);
CAPI VALUE snow_arguments_set_by_index(SnArguments*, uintx idx, VALUE val);
CAPI VALUE snow_arguments_push(SnArguments*, VALUE val)                         ATTR_HOT;
CAPI VALUE snow_arguments_push_named(SnArguments*, SnSymbol name, VALUE val)    ATTR_HOT;
CAPI intx snow_arguments_add_name(SnArguments*, SnSymbol name)                  ATTR_HOT;
CAPI intx snow_arguments_get_index_for_name(SnArguments*, SnSymbol name)        ATTR_HOT;
CAPI VALUE snow_arguments_get_by_index(SnArguments*, intx idx)                  ATTR_HOT;
CAPI VALUE snow_arguments_get_by_name(SnArguments*, SnSymbol sym)               ATTR_HOT;

#endif /* end of include guard: ARGUMENTS_H_4ZC67CWW */
