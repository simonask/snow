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

SnArray* snow_copy_array(VALUE* data, uintx size) {
	SnArray* array = snow_create_array_with_size(size);
	for (uintx i = 0; i < size; ++i) {
		INTERN->data[i] = data[i];
	}
	INTERN->size = size;
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

SNOW_FUNC(_array_any) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	return boolean_to_value(snow_array_size((SnArray*)SELF) > 0);
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
		
		const char* converted = snow_value_to_cstr(snow_call_method(array_get(INTERN, i), inspect, 0));
		
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

static void _array_each_parallel_callback(void* _data, size_t element_size, size_t i, void* userdata) {
	ASSERT(element_size == sizeof(VALUE));
	VALUE* data = (VALUE*)_data;
	snow_call(NULL, *(void**)userdata, 2, data[i], int_to_value(i));
}

SNOW_FUNC(_array_each_parallel) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	REQUIRE_ARGS(1);
	SnArray* array = (SnArray*)SELF;
	VALUE closure = ARGS[0];
	
	// TODO: Freeze array
	snow_parallel_for_each(INTERN->data, sizeof(VALUE), array_size(INTERN), _array_each_parallel_callback, &closure);
	
	return array;
}

SNOW_FUNC(_array_map) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	REQUIRE_ARGS(1);
	SnArray* array = (SnArray*)SELF;
	VALUE closure = ARGS[0];
	SnArray* new_array = snow_create_array_with_size(snow_array_size(array));
	for (intx i = 0; i < array_size(INTERN); ++i)
	{
		snow_array_set(new_array, i, snow_call(NULL, closure, 1, array_get(INTERN, i)));
	}
	return new_array;
}

struct ParallelMapUserdata {
	VALUE closure;
	SnArray* new_array;
};

static void _array_map_parallel_callback(void* _data, size_t element_size, size_t i, void* _userdata) {
	ASSERT(_userdata);
	ASSERT(element_size == sizeof(VALUE));
	VALUE* data = (VALUE*)_data;
	struct ParallelMapUserdata* userdata = (struct ParallelMapUserdata*)_userdata;
	snow_array_set(userdata->new_array, i, snow_call(NULL, userdata->closure, 2, data[i], int_to_value(i)));
}

SNOW_FUNC(_array_map_parallel) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	REQUIRE_ARGS(1);
	SnArray* array = (SnArray*)SELF;
	VALUE closure = ARGS[0];
	SnArray* new_array = snow_create_array_with_size(snow_array_size(array));
	
	struct ParallelMapUserdata userdata;
	userdata.closure = closure;
	userdata.new_array = new_array;

	// TODO: Freeze array
	snow_parallel_for_each(INTERN->data, sizeof(VALUE), array_size(INTERN), _array_map_parallel_callback, &userdata);
	
	return new_array;
}

SNOW_FUNC(_array_select) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	REQUIRE_ARGS(1);
	SnArray* array = (SnArray*)SELF;
	VALUE closure = ARGS[0];
	SnArray* new_array = snow_create_array_with_size(snow_array_size(array));
	
	for (intx i = 0; i < snow_array_size(array); ++i) {
		if (snow_eval_truth(snow_call(NULL, closure, 2, INTERN->data[i], int_to_value(i)))) {
			snow_array_push(new_array, INTERN->data[i]);
		}
	}
	
	return new_array;
}

SNOW_FUNC(_array_reject) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	REQUIRE_ARGS(1);
	SnArray* array = (SnArray*)SELF;
	VALUE closure = ARGS[0];
	SnArray* new_array = snow_create_array_with_size(snow_array_size(array));
	
	for (intx i = 0; i < snow_array_size(array); ++i) {
		if (!snow_eval_truth(snow_call(NULL, closure, 2, INTERN->data[i], int_to_value(i)))) {
			snow_array_push(new_array, INTERN->data[i]);
		}
	}
	
	return new_array;
}

SNOW_FUNC(_array_inject) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	REQUIRE_ARGS(1);
	SnArray* array = (SnArray*)SELF;
	VALUE closure = ARGS[0];
	
	intx len = snow_array_size(array);
	if (len > 0)
	{
		VALUE carry = INTERN->data[0];
		for (intx i = 1; i < len; ++i) {
			carry = snow_call(NULL, closure, 3, carry, INTERN->data[i], int_to_value(i));
		}
		return carry;
	}
	return SN_NIL;
}

SNOW_FUNC(_array_detect) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	REQUIRE_ARGS(1);
	SnArray* array = (SnArray*)SELF;
	VALUE closure = ARGS[0];
	
	for (size_t i = 0; i < snow_array_size(array); ++i) {
		VALUE x = snow_array_get(array, i);
		if (snow_eval_truth(snow_call(NULL, closure, 1, x)))
			return x;
	}
	return SN_NIL;
}

SNOW_FUNC(_array_join) {
	ASSERT_TYPE(SELF, SN_ARRAY_TYPE);
	SnArray* array = (SnArray*)SELF;
	SnString* separator = NUM_ARGS > 0 ? (SnString*)snow_call_method(ARGS[0], snow_symbol("to_string"), 0) : NULL;
	if (separator) { ASSERT_TYPE(separator, SN_STRING_TYPE); }
	SnString* result = snow_create_string("");
	for (size_t i = 0; i < snow_array_size(array); ++i) {
		SnString* converted = (SnString*)snow_call_method(snow_array_get(array, i), snow_symbol("to_string"), 0);
		ASSERT_TYPE(converted, SN_STRING_TYPE);
		result = snow_string_concatenate(result, converted);
		if (i < snow_array_size(array)-1 && separator)
			result = snow_string_concatenate(result, separator);
	}
	return result;
}

void init_array_class(SnClass* klass)
{
	snow_define_class_method(klass, "__call__", _array_new);
	snow_define_method(klass, "get", _array_get);
	snow_define_method(klass, "[]", _array_get);
	snow_define_method(klass, "set", _array_set);
	snow_define_method(klass, "[]:", _array_set);
	snow_define_method(klass, "push", _array_push);
	snow_define_method(klass, "<<", _array_push);
	snow_define_method(klass, "pop", _array_pop);
	snow_define_method(klass, "inspect", _array_inspect);
	snow_define_method(klass, "to_string", _array_inspect);
	snow_define_method(klass, "each", _array_each);
	snow_define_method(klass, "each_parallel", _array_each_parallel);
	snow_define_method(klass, "map", _array_map);
	snow_define_method(klass, "map_parallel", _array_map_parallel);
	snow_define_method(klass, "select", _array_select);
	snow_define_method(klass, "reject", _array_reject);
	snow_define_method(klass, "inject", _array_inject);
	snow_define_method(klass, "detect", _array_detect);
	snow_define_method(klass, "join", _array_join);
	
	snow_define_property(klass, "length", _array_length, NULL);
	snow_define_property(klass, "any?", _array_any, NULL);
}
