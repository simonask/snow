#include "snow/array.h"
#include "snow/intern.h"
#include "snow/gc.h"
#include <string.h>

SnArray* snow_create_array() {
	SnArray* array = (SnArray*)snow_alloc_any_object(SN_ARRAY_TYPE, sizeof(SnArray));
	array->alloc_size = 0;
	array->size = 0;
	array->data = NULL;
	return array;
}

SnArray* snow_create_array_with_size(uintx size) {
	SnArray* array = snow_create_array();
	array->alloc_size = size;
	array->data = snow_gc_alloc(size * sizeof(VALUE));
	memset(array->data, '\0', size * sizeof(VALUE));
	return array;
}

VALUE snow_array_get(SnArray* array, intx index) {
	if (index >= array->size || index < 0) {
		// XXX: TODO
		return SN_NIL;
	}
	return array->data[index];
}

VALUE snow_array_set(SnArray* array, intx index, VALUE value) {
	if (index >= array->size || index < 0) {
		// XXX: TODO
		return SN_NIL;
	}
	array->data[index] = value;
	return value;
}

VALUE snow_array_push(SnArray* array, VALUE value) {
	if (array->alloc_size <= array->size) {
		// realloc
		uintx new_size = array->alloc_size * 2;
		VALUE* new_data = snow_gc_alloc(new_size * sizeof(VALUE));
		memset(new_data, '\0', new_size * sizeof(VALUE));
		memcpy(new_data, array->data, array->size * sizeof(VALUE));
		array->data = new_data;
		array->alloc_size = new_size;
	}
	
	array->data[array->size++] = value;
	return value;
}
