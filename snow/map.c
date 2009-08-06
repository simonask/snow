#include "snow/map.h"
#include "snow/gc.h"
#include "snow/intern.h"

typedef struct MapTuple {
	VALUE key, value;
} MapTuple;

SnMap* snow_create_map()
{
	SnMap* map = (SnMap*)snow_alloc_any_object(SN_MAP_TYPE, sizeof(SnMap));
	map->size = 0;
	map->data = NULL;
	map->compare = snow_map_compare_default;
	return map;
}

SnMap* snow_create_map_with_compare(SnMapCompare cmp)
{
	SnMap* map = snow_create_map();
	map->compare = cmp;
	return map;
}

bool snow_map_contains(SnMap* map, VALUE key)
{
	return snow_map_get(map, key) != NULL;
}

VALUE snow_map_get(SnMap* map, VALUE key)
{
	uintx i;
	MapTuple* tuples = (MapTuple*)map->data;
	for (i = 0; i < map->size; ++i) {
		if (map->compare(tuples[i].key, key) == 0)
			return tuples[i].value;
	}
	return NULL;
}

VALUE snow_map_set(SnMap* map, VALUE key, VALUE value)
{
	ASSERT(key && value && "Cannot use NULL as keys/values in Snow maps!");
	uintx i;
	MapTuple* tuples = (MapTuple*)map->data;
	for (i = 0; i < map->size; ++i) {
		if (map->compare(tuples[i].key, key) == 0)
		{
			tuples[i].value = value;
			return value;
		}
	}
	
	// TODO: preallocate
	void* new_data = snow_gc_alloc(sizeof(MapTuple) * (map->size + 1));
	memcpy(new_data, map->data, map->size * sizeof(MapTuple));
	i = map->size++;
	map->data = new_data;
	tuples = (MapTuple*)map->data;
	tuples[i].key = key;
	tuples[i].value = value;
	return value;
}

int snow_map_compare_default(VALUE a, VALUE b)
{
	return (intx)b - (intx)a;
}

void init_map_class(SnClass* klass)
{
	
}

