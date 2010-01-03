#include "snow/map.h"
#include "snow/gc.h"
#include "snow/snow.h"
#include "snow/class.h"
#include "snow/intern.h"

#include <stdio.h>

typedef struct SnMapTuple {
	VALUE key, value;
} SnMapTuple;

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

SnMap* snow_create_map_with_deep_comparison()
{
	return snow_create_map_with_compare(snow_compare_objects);
}

bool snow_map_contains(SnMap* map, VALUE key)
{
	return snow_map_get(map, key) != NULL;
}

VALUE snow_map_get(SnMap* map, VALUE key)
{
	uintx i;
	SnMapTuple* tuples = map->data;
	for (i = 0; i < map->size; ++i) {
		if (map->compare(tuples[i].key, key) == 0)
			return tuples[i].value;
	}
	return NULL;
}

void snow_map_set(SnMap* map, VALUE key, VALUE value)
{
	ASSERT(key && "Cannot use NULL as key in Snow maps!");
	uintx i;
	SnMapTuple* tuples = map->data;
	for (i = 0; i < map->size; ++i) {
		if (map->compare(tuples[i].key, key) == 0)
		{
			tuples[i].value = value;
			return;
		}
	}
	
	// TODO: preallocate
	void* new_data = snow_gc_alloc(sizeof(SnMapTuple) * (map->size + 1));
	memcpy(new_data, map->data, map->size * sizeof(SnMapTuple));
	i = map->size++;
	map->data = new_data;
	tuples = map->data;
	tuples[i].key = key;
	tuples[i].value = value;
}

int snow_map_compare_default(VALUE a, VALUE b)
{
	return (intx)b - (intx)a;
}

SNOW_FUNC(map_new) {
	SnMap* map = snow_create_map();
	snow_arguments_put_in_map(_context->args, map);
	return map;
}

SNOW_FUNC(map_keys) {
	ASSERT_TYPE(SELF, SN_MAP_TYPE);
	SnMap* self = (SnMap*)SELF;
	SnMapTuple* tuples = self->data;
	SnArray* keys = snow_create_array();
	
	uintx i;
	for (i = 0; i < self->size; ++i) {
		snow_array_push(keys, tuples[i].key);
	}
	
	return keys;
}

SNOW_FUNC(map_values) {
	ASSERT_TYPE(SELF, SN_MAP_TYPE);
	SnMap* self = (SnMap*)SELF;
	SnMapTuple* tuples = self->data;
	SnArray* values = snow_create_array();
	
	uintx i;
	for (i = 0; i < self->size; ++i) {
		snow_array_push(values, tuples[i].value);
	}
	
	return values;
}

SNOW_FUNC(map_get) {
	REQUIRE_ARGS(1);
	ASSERT_TYPE(SELF, SN_MAP_TYPE);
	return snow_map_get(SELF, ARGS[0]);
}

SNOW_FUNC(map_set) {
	REQUIRE_ARGS(2);
	ASSERT_TYPE(SELF, SN_MAP_TYPE);
	snow_map_set(SELF, ARGS[0], ARGS[1]);
	return ARGS[1];
}

static inline SnString* call_inspect(VALUE self) {
	static bool got_sym = false;
	static SnSymbol inspect;
	if (!got_sym) { inspect = snow_symbol("inspect"); got_sym = true; }
	SnString* str = snow_call_method(self, inspect, 0);
	ASSERT_TYPE(str, SN_STRING_TYPE);
	return str;
}

SNOW_FUNC(map_inspect) {
	ASSERT_TYPE(SELF, SN_MAP_TYPE);
	SnMap* self = (SnMap*)SELF;
	SnArray* strings = snow_create_array_with_size(self->size);
	for (size_t i = 0; i < self->size; ++i) {
		SnString* key = call_inspect(self->data[i].key);
		SnString* value = call_inspect(self->data[i].value);
		char* str;
		asprintf(&str, "%s => %s", snow_string_cstr(key), snow_string_cstr(value));
		ASSERT(str);
		snow_array_push(strings, snow_create_string(str));
		free(str);
	}
	
	SnString* joined = snow_array_join(strings, ", ");
	snow_array_clear(strings);
	snow_array_push(strings, snow_create_string("Map("));
	snow_array_push(strings, joined);
	snow_array_push(strings, snow_create_string(")"));
	return snow_array_join(strings, NULL);
}

void init_map_class(SnClass* klass)
{
	snow_define_class_method(klass, "__call__", map_new);
	
	snow_define_property(klass, "keys", map_keys, NULL);
	snow_define_property(klass, "values", map_values, NULL);
	
	snow_define_method(klass, "[]", map_get);
	snow_define_method(klass, "[]:", map_set);
	snow_define_method(klass, "inspect", map_inspect);
}
