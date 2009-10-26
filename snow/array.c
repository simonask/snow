#include "snow/array.h"
#include "snow/intern.h"
#include "snow/gc.h"
#include "snow/class.h"
#include "snow/array-intern.h"
#include "snow/continuation.h"
#include <string.h>
#include <stdio.h>

#define INTERN (&array->a)

SnArray* snow_create_array() {
	SnArray* array = (SnArray*)snow_alloc_any_object(SN_ARRAY_TYPE, sizeof(SnArray));
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
	return array_pop(INTERN);
}

intx snow_array_find(SnArray* array, VALUE val) {
	return array_find(INTERN, val);
}

intx snow_array_find_or_add(SnArray* array, VALUE val) {
	return array_find_or_add(INTERN, val);
}

SNOW_FUNC(_array_new) {
	SnArray* array = snow_create_array_with_size(NUM_ARGS);
	for (uintx i = 0; i < NUM_ARGS; ++i) {
		snow_array_set(array, i, ARGS[i]);
	}
	return array;
}

SNOW_FUNC(_array_get) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	REQUIRE_ARGS(1);
	ASSERT(is_integer(ARGS[0]));
	intx i = value_to_int(ARGS[0]);
	VALUE v = snow_array_get((SnArray*)SELF, i);
	return v ? v : SN_NIL;
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

SNOW_FUNC(_array_length) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	return int_to_value(snow_array_size((SnArray*)SELF));
}

SNOW_FUNC(_array_inspect) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	SnArray* array = (SnArray*)SELF;
	
	SnSymbol inspect = snow_symbol("inspect");
	
	// TODO: Use internal snow string concatenation, and generally rewrite this mess
	char* result = NULL;
	for (intx i = 0; i < array_size(INTERN); ++i)
	{
		char* old_result = result;
		
		const char* converted = snow_value_to_string(snow_call_method(array_get(INTERN, i), inspect, 0));
		
		if (!old_result)
		{
			asprintf(&result, "%s", converted);
		}
		else
		{
			asprintf(&result, "%s, %s", old_result, converted);
			snow_free(old_result);
		}
	}
	
	char* old_result = result;
	asprintf(&result, "@(%s)", result ? result : "");
	snow_free(old_result);
	SnString* str = snow_create_string(result);
	snow_free(result);
	return str;
}

SNOW_FUNC(_array_each) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	REQUIRE_ARGS(1);
	SnArray* array = (SnArray*)SELF;
	VALUE closure = ARGS[0];
	
	for (intx i = 0; i < array_size(INTERN); ++i)
	{
		snow_call(NULL, closure, 1, array_get(INTERN, i));
	}
	
	return array;
}

void init_array_class(SnClass* klass)
{
	snow_define_class_method(klass, "__call__", _array_new);
	snow_define_method(klass, "get", _array_get);
	snow_define_method(klass, "set", _array_set);
	snow_define_method(klass, "push", _array_push);
	snow_define_method(klass, "<<", _array_push);
	snow_define_method(klass, "pop", _array_pop);
	snow_define_method(klass, "inspect", _array_inspect);
	snow_define_method(klass, "each", _array_each);
	
	snow_define_property(klass, "length", _array_length, NULL);
}
