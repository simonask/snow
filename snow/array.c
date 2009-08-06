#include "snow/array.h"
#include "snow/intern.h"
#include "snow/gc.h"
#include "snow/class.h"
#include <string.h>

SnArray* snow_create_array() {
	SnArray* array = (SnArray*)snow_alloc_any_object(SN_ARRAY_TYPE, sizeof(SnArray));
	snow_object_init((SnObject*)array, NULL);
	array->alloc_size = 0;
	array->size = 0;
	array->data = NULL;
	return array;
}

SnArray* snow_create_array_with_size(uintx size) {
	SnArray* array = snow_create_array();
	if (!size) size = 8;
	array->alloc_size = size;
	array->size = 0;
	array->data = snow_gc_alloc(size * sizeof(VALUE));
	return array;
}

SnArray* snow_ref_array(VALUE* data, uintx size) {
	SnArray* ar = snow_create_array();
	ar->alloc_size = 0;
	ar->size = size;
	ar->data = data;
	return ar;
}

VALUE snow_array_get(SnArray* array, intx idx) {
	if (idx >= array->size || idx < 0) {
		// FIXME: wrap-around for negative indices
		return SN_NIL;
	}
	return array->data[idx];
}

VALUE snow_array_set(SnArray* array, intx idx, VALUE value) {
	if (idx >= array->size || idx < 0) {
		uintx new_size = idx+1;
		VALUE* new_data = (VALUE*)snow_gc_alloc(new_size * sizeof(VALUE));
		for (uintx i = array->size; i < new_size; ++i) {
			new_data[i] = SN_NIL;
		}
		memcpy(new_data, array->data, array->size * sizeof(VALUE));
		array->size = new_size;
		array->data = new_data;
	}
	array->data[idx] = value;
	return value;
}

VALUE snow_array_push(SnArray* array, VALUE value) {
	// TODO: mutex lock
	if (array->alloc_size <= array->size) {
		// realloc
		uintx new_size = array->alloc_size * 2;
		if (!new_size) new_size = 8;
		VALUE* new_data = snow_gc_alloc(new_size * sizeof(VALUE));
		memset(new_data, '\0', new_size * sizeof(VALUE));
		memcpy(new_data, array->data, array->size * sizeof(VALUE));
		array->data = new_data;
		array->alloc_size = new_size;
	}
	
	array->data[array->size++] = value;
	return value;
}

VALUE snow_array_pop(SnArray* array) {
	if (array->size > 0)
		return array->data[--array->size];
	else
		return SN_NIL;
}

intx snow_array_find(SnArray* array, VALUE val) {
	for (intx i = 0; i < array->size; ++i) {
		if (array->data[i] == val)
			return i;
	}
	return -1;
}

SNOW_FUNC(array_new) {
	SnArray* ar = snow_create_array_with_size(NUM_ARGS);
	for (uintx i = 0; i < NUM_ARGS; ++i) {
		ar->data[i] = ARGS[i];
	}
	return ar;
}

SNOW_FUNC(array_get) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	REQUIRE_ARGS(1);
	ASSERT(is_integer(ARGS[0]));
	intx i = value_to_int(ARGS[0]);
	return snow_array_get((SnArray*)SELF, i);
}

SNOW_FUNC(array_set) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	REQUIRE_ARGS(2);
	ASSERT(is_integer(ARGS[0]));
	intx i = value_to_int(ARGS[0]);
	return snow_array_set((SnArray*)SELF, i, ARGS[1]);
}

SNOW_FUNC(array_push) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	REQUIRE_ARGS(1);
	return snow_array_push((SnArray*)SELF, ARGS[0]);
}

SNOW_FUNC(array_pop) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	return snow_array_pop((SnArray*)SELF);
}

void init_array_class(SnClass* klass)
{
	snow_define_class_method(klass, "__call__", array_new);
	snow_define_method(klass, "get", array_get);
	snow_define_method(klass, "set", array_set);
	snow_define_method(klass, "push", array_push);
	snow_define_method(klass, "pop", array_pop);
}
