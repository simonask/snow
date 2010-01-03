#ifndef MAP_H_VZXFJ4GY
#define MAP_H_VZXFJ4GY

#include "snow/basic.h"
#include "snow/object.h"

typedef int(*SnMapCompare)(VALUE a, VALUE b);

struct SnMapTuple;

typedef struct SnMap {
	SnObjectBase base;
	uintx size;
	struct SnMapTuple* data;
	SnMapCompare compare;
} SnMap;

CAPI SnMap* snow_create_map();
CAPI SnMap* snow_create_map_with_compare(SnMapCompare);
CAPI SnMap* snow_create_map_with_deep_comparison();
CAPI bool snow_map_contains(SnMap*, VALUE key);
CAPI VALUE snow_map_get(SnMap*, VALUE key);
CAPI void snow_map_set(SnMap*, VALUE key, VALUE val);

CAPI int snow_map_compare_default(VALUE, VALUE);

static inline uintx snow_map_size(SnMap* map) { return map->size; }

#endif /* end of include guard: MAP_H_VZXFJ4GY */
