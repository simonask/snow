#include "snow/array.h"
#include "snow/intern.h"
#include "snow/gc.h"
#include "snow/class.h"
#include "snow/array-intern.h"
#include <string.h>

#define INTERN (&array->a)

SnArray* snow_create_array() {
	SnArray* array = (SnArray*)snow_alloc_any_object(SN_ARRAY_TYPE, sizeof(SnArray));
	snow_object_init((SnObject*)array, NULL);
	array_init(INTERN);
	return array;
}

SnArray* snow_create_array_with_size(uintx size) {
	SnArray* array = snow_create_array();
	if (!size) size = 8;
	array_init_with_size(INTERN, size);
	return array;
}

SnArray* snow_ref_array(VALUE* data, uintx size) {
	SnArray* array = snow_create_array();
	array_init(INTERN);
	INTERN->alloc_size = 0;
	INTERN->size = size;
	INTERN->data = data;
	return array;
}

VALUE snow_array_get(SnArray* array, intx idx) {
	return array_get(INTERN, idx);
}

VALUE snow_array_set(SnArray* array, intx idx, VALUE value) {
	return array_set(INTERN, idx, value);
}

VALUE snow_array_push(SnArray* array, VALUE value) {
	// TODO: mutex lock
	return array_set(INTERN, INTERN->size, value);
}

VALUE snow_array_pop(SnArray* array) {
	if (INTERN->size > 0)
		return INTERN->data[--INTERN->size];
	else
		return SN_NIL;
}

intx snow_array_find(SnArray* array, VALUE val) {
	for (inth i = 0; i < INTERN->size; ++i) {
		if (INTERN->data[i] == val)
			return (intx)i;
	}
	return -1;
}

SNOW_FUNC(_array_new) {
	SnArray* array = snow_create_array_with_size(NUM_ARGS);
	for (uintx i = 0; i < NUM_ARGS; ++i) {
		INTERN->data[i] = ARGS[i];
	}
	return array;
}

SNOW_FUNC(_array_get) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	REQUIRE_ARGS(1);
	ASSERT(is_integer(ARGS[0]));
	intx i = value_to_int(ARGS[0]);
	return snow_array_get((SnArray*)SELF, i);
}

SNOW_FUNC(_array_set) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	REQUIRE_ARGS(2);
	ASSERT(is_integer(ARGS[0]));
	intx i = value_to_int(ARGS[0]);
	return snow_array_set((SnArray*)SELF, i, ARGS[1]);
}

SNOW_FUNC(_array_push) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	REQUIRE_ARGS(1);
	return snow_array_push((SnArray*)SELF, ARGS[0]);
}

SNOW_FUNC(_array_pop) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	return snow_array_pop((SnArray*)SELF);
}

void init_array_class(SnClass* klass)
{
	snow_define_class_method(klass, "__call__", _array_new);
	snow_define_method(klass, "get", _array_get);
	snow_define_method(klass, "set", _array_set);
	snow_define_method(klass, "push", _array_push);
	snow_define_method(klass, "pop", _array_pop);
}
