#include "snow/array.h"
#include "snow/intern.h"
#include "snow/gc.h"
#include "snow/class.h"
#include "snow/array-intern.h"
#include "snow/continuation.h"
#include <string.h>
#include <stdio.h>

SnArray* snow_create_array() {
	SnArray* array = SNOW_GC_ALLOC_OBJECT(SnArray);
	array->data = NULL;
	array->size = 0;
	array->alloc_size = 0;
	array->lock = snow_rwlock_create();
	return array;
}

SnArray* snow_create_fixed_array(uintx sz) {
	SnArray* array = SNOW_GC_ALLOC_OBJECT(SnArray);
	array->data = sz ? (VALUE*)snow_malloc(sizeof(VALUE)*sz) : NULL;
	array->size = 0;
	array->alloc_size = sz;
	array->lock = NULL; // fixed arrays don't need locks => faster!
	return array;
}

SnArray* snow_create_array_with_size(uintx size) {
	SnArray* array = snow_create_array();
	if (size) {
		array->data = snow_malloc(sizeof(VALUE)*size);
		array->alloc_size = size;
	}
	return array;
}

void SnArray_finalize(SnArray* array) {
	snow_free(array->data);
	snow_rwlock_destroy(array->lock);
}

SnArray* snow_copy_array(VALUE* data, uintx size) {
	SnArray* array = snow_create_array_with_size(size);
	for (uintx i = 0; i < size; ++i) {
		array->data[i] = data[i];
	}
	array->size = size;
	return array;
}

void snow_array_grow(SnArray* array, intx new_size)
{
	if (new_size > array->alloc_size)
	{
		ASSERT(array->lock); // can't grow a fixed array beyond preallocated size! TODO: Make this an exception.
		snow_rwlock_write(array->lock);
		array->data = snow_realloc(array->data, sizeof(VALUE)*new_size);
		array->alloc_size = new_size;
		snow_rwlock_unlock(array->lock);
	}
	
	if (new_size > array->size)
	{
		for (intx i = array->size; i < new_size; ++i)
			array->data[i] = SN_NIL;
		array->size = new_size;
	}
}

VALUE snow_array_get(SnArray* array, intx idx)
{
	// WARNING: This is explicitly inlined in codegens. If you change this, you must also change each codegen that inlines this.
	if (idx >= array->size)
		return NULL;
	if (idx < 0)
		idx += array->size;
	if (idx < 0)
		return NULL;
	snow_rwlock_read(array->lock);
	VALUE val = array->data[idx];
	snow_rwlock_unlock(array->lock);
	return val;
}

VALUE snow_array_set(SnArray* array, intx idx, VALUE val)
{
	if (idx >= array->size)
		snow_array_grow(array, idx+1);
	if (idx < 0)
		idx += array->size;
	if (idx < 0)
		TRAP(); // index out of bounds
	snow_rwlock_read(array->lock);
	array->data[idx] = val;
	snow_rwlock_unlock(array->lock);
	return val;
}

VALUE snow_array_erase(SnArray* array, intx idx)
{
	if (idx >= array->size)
		snow_array_grow(array, idx+1);
	if (idx < 0)
		idx += array->size;
	if (idx < 0)
		TRAP(); // index out of bounds
	snow_rwlock_write(array->lock);
	VALUE val = array->data[idx];
	for (intx i = idx; i < array->size - 1; ++i) {
		array->data[i] = array->data[i+1];
	}
	array->data[--array->size] = NULL;
	snow_rwlock_unlock(array->lock);
	return val;
}

VALUE snow_array_push(SnArray* array, VALUE val)
{
	return snow_array_set(array, snow_array_size(array), val);
}

VALUE snow_array_pop(SnArray* array)
{
	if (array->size)
	{
		snow_rwlock_write(array->lock);
		VALUE val = array->data[--array->size];
		array->data[array->size] = SN_NIL; // for gc
		snow_rwlock_unlock(array->lock);
		return val;
	}
	else
		return NULL;
}

intx snow_array_find(SnArray* array, VALUE val)
{
	intx idx = -1;
	snow_rwlock_read(array->lock);
	for (intx i = 0; i < array->size; ++i)
	{
		if (array->data[i] == val) {
			idx = i;
			break;
		}
	}
	snow_rwlock_unlock(array->lock);
	return idx;
}

intx snow_array_find_with_compare(SnArray* array, VALUE val, SnCompareFunc cmp)
{
	int idx = -1;
	snow_rwlock_read(array->lock);
	for (intx i = 0; i < array->size; ++i)
	{
		if (cmp(array->data[i], val) == 0) {
			idx = 0;
			break;
		}
	}
	snow_rwlock_unlock(array->lock);
	return idx;
}

intx snow_array_find_or_add(SnArray* array, VALUE val)
{
	int i = snow_array_find(array, val);
	if (i >= 0) return i;
	i = snow_array_size(array);
	snow_array_push(array, val);
	return i;
}

void snow_array_clear(SnArray* array)
{
	snow_rwlock_write(array->lock);
	if (array->data) {
		memset(array->data, 0, array->size * sizeof(VALUE));
	}
	array->size = 0;
	snow_rwlock_unlock(array->lock);
}

SnString* snow_array_join(SnArray* array, const char* sep) {
	if (!sep) sep = "";
	uintx sep_len = strlen(sep);
	
	uintx num_strings = snow_array_size(array);
	SnString* strings[num_strings];
	uintx strings_len = 0;
	for (size_t i = 0; i < num_strings; ++i) {
		VALUE x = snow_array_get(array, i);
		strings[i] = snow_typeof(x) == SnStringType ? (SnString*)x : (SnString*)snow_call_method(snow_array_get(array, i), snow_symbol("to_string"), 0);
		strings_len += snow_string_size(strings[i]);
	}
	
	char buffer[strings_len + (num_strings-1)*sep_len];
	char* p = buffer;
	for (size_t i = 0; i < num_strings; ++i) {
		uintx len = snow_string_size(strings[i]);
		memcpy(p, snow_string_cstr(strings[i]), len);
		p += len;
		if (i < num_strings-1) {
			memcpy(p, sep, sep_len);
			p += sep_len;
		}
	}
	*p = '\0';
	
	return snow_create_string_from_data((byte*)buffer, p - buffer);
}

void snow_array_parallel_for_each(SnArray* array, SnParallelForEachCallback callback, void* userdata) {
	snow_parallel_for_each(array->data, sizeof(VALUE), array->size, callback, userdata);
}

SNOW_FUNC(_array_new) {
	SnArray* array = snow_create_array_with_size(NUM_ARGS);
	for (uintx i = 0; i < NUM_ARGS; ++i) {
		snow_array_set(array, i, ARGS[i]);
	}
	return array;
}

SNOW_FUNC(_array_get) {
	ASSERT_TYPE(SELF, SnArrayType);
	REQUIRE_ARGS(1);
	ASSERT(snow_is_integer(ARGS[0]));
	intx i = snow_value_to_int(ARGS[0]);
	VALUE v = snow_array_get((SnArray*)SELF, i);
	return v ? v : SN_NIL;
}

SNOW_FUNC(_array_set) {
	ASSERT_TYPE(SELF, SnArrayType);
	REQUIRE_ARGS(2);
	ASSERT(snow_is_integer(ARGS[0]));
	intx i = snow_value_to_int(ARGS[0]);
	return snow_array_set((SnArray*)SELF, i, ARGS[1]);
}

SNOW_FUNC(_array_push) {
	ASSERT_TYPE(SELF, SnArrayType);
	REQUIRE_ARGS(1);
	return snow_array_push((SnArray*)SELF, ARGS[0]);
}

SNOW_FUNC(_array_pop) {
	ASSERT_TYPE(SELF, SnArrayType);
	return snow_array_pop((SnArray*)SELF);
}

SNOW_FUNC(_array_equals) {
	ASSERT_TYPE(SELF, SnArrayType);
	REQUIRE_ARGS(1);
	
	if (snow_typeof(ARGS[0]) != SnArrayType)
		return SN_FALSE;
	
	SnArray* self = (SnArray*)SELF;
	SnArray* other = (SnArray*)ARGS[0];
	
	size_t size = snow_array_size(self);
	if (size != snow_array_size(other))
		return SN_FALSE;
	
	for (size_t i = 0; i < size; ++i) {
		VALUE left = snow_array_get(self, i), right = snow_array_get(other, i);
		VALUE comparison = snow_call_method(left, snow_symbol("="), 1, right);
		if (!snow_eval_truth(comparison))
			return SN_FALSE;
	}
	
	return SN_TRUE;
}

SNOW_FUNC(_array_length) {
	ASSERT_TYPE(SELF, SnArrayType);
	return snow_int_to_value(snow_array_size((SnArray*)SELF));
}

SNOW_FUNC(_array_any) {
	ASSERT_TYPE(SELF, SnArrayType);
	return snow_boolean_to_value(snow_array_size((SnArray*)SELF) > 0);
}

SNOW_FUNC(_array_inspect) {
	ASSERT_TYPE(SELF, SnArrayType);
	SnArray* array = (SnArray*)SELF;
	
	SnSymbol inspect = snow_symbol("inspect");
	
	// TODO: Use internal snow string concatenation, and generally rewrite this mess
	char* result = NULL;
	for (intx i = 0; i < snow_array_size(array); ++i)
	{
		char* old_result = result;
		
		const char* converted = snow_value_to_cstr(snow_call_method(snow_array_get(array, i), inspect, 0));
		
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
	ASSERT_TYPE(SELF, SnArrayType);
	REQUIRE_ARGS(1);
	SnArray* array = (SnArray*)SELF;
	VALUE closure = ARGS[0];
	
	for (intx i = 0; i < snow_array_size(array); ++i)
	{
		snow_call(NULL, closure, 2, snow_array_get(array, i), snow_int_to_value(i));
	}
	
	return array;
}

static void _array_each_parallel_callback(void* _data, size_t element_size, size_t i, void* userdata) {
	ASSERT(element_size == sizeof(VALUE));
	VALUE* data = (VALUE*)_data;
	snow_call(NULL, *(void**)userdata, 2, data[i], snow_int_to_value(i));
}

SNOW_FUNC(_array_each_parallel) {
	ASSERT_TYPE(SELF, SnArrayType);
	REQUIRE_ARGS(1);
	SnArray* array = (SnArray*)SELF;
	VALUE closure = ARGS[0];
	
	// TODO: Freeze array
	snow_parallel_for_each(array->data, sizeof(VALUE), snow_array_size(array), _array_each_parallel_callback, &closure);
	
	return array;
}

SNOW_FUNC(_array_map) {
	ASSERT_TYPE(SELF, SnArrayType);
	REQUIRE_ARGS(1);
	SnArray* array = (SnArray*)SELF;
	VALUE closure = ARGS[0];
	SnArray* new_array = snow_create_array_with_size(snow_array_size(array));
	for (intx i = 0; i < snow_array_size(array); ++i)
	{
		snow_array_set(new_array, i, snow_call(NULL, closure, 1, snow_array_get(array, i)));
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
	snow_array_set(userdata->new_array, i, snow_call(NULL, userdata->closure, 2, data[i], snow_int_to_value(i)));
}

SNOW_FUNC(_array_map_parallel) {
	ASSERT_TYPE(SELF, SnArrayType);
	REQUIRE_ARGS(1);
	SnArray* array = (SnArray*)SELF;
	VALUE closure = ARGS[0];
	SnArray* new_array = snow_create_array_with_size(snow_array_size(array));
	
	struct ParallelMapUserdata userdata;
	userdata.closure = closure;
	userdata.new_array = new_array;

	// TODO: Freeze array
	snow_parallel_for_each(array->data, sizeof(VALUE), snow_array_size(array), _array_map_parallel_callback, &userdata);
	
	return new_array;
}

SNOW_FUNC(_array_select) {
	ASSERT_TYPE(SELF, SnArrayType);
	REQUIRE_ARGS(1);
	SnArray* array = (SnArray*)SELF;
	VALUE closure = ARGS[0];
	SnArray* new_array = snow_create_array_with_size(snow_array_size(array));
	
	for (intx i = 0; i < snow_array_size(array); ++i) {
		if (snow_eval_truth(snow_call(NULL, closure, 2, array->data[i], snow_int_to_value(i)))) {
			snow_array_push(new_array, array->data[i]);
		}
	}
	
	return new_array;
}

SNOW_FUNC(_array_reject) {
	ASSERT_TYPE(SELF, SnArrayType);
	REQUIRE_ARGS(1);
	SnArray* array = (SnArray*)SELF;
	VALUE closure = ARGS[0];
	SnArray* new_array = snow_create_array_with_size(snow_array_size(array));
	
	for (intx i = 0; i < snow_array_size(array); ++i) {
		if (!snow_eval_truth(snow_call(NULL, closure, 2, array->data[i], snow_int_to_value(i)))) {
			snow_array_push(new_array, array->data[i]);
		}
	}
	
	return new_array;
}

SNOW_FUNC(_array_inject) {
	ASSERT_TYPE(SELF, SnArrayType);
	REQUIRE_ARGS(1);
	SnArray* array = (SnArray*)SELF;
	VALUE closure = ARGS[0];
	
	intx len = snow_array_size(array);
	if (len > 0)
	{
		VALUE carry = array->data[0];
		for (intx i = 1; i < len; ++i) {
			carry = snow_call(NULL, closure, 3, carry, array->data[i], snow_int_to_value(i));
		}
		return carry;
	}
	return SN_NIL;
}

SNOW_FUNC(_array_detect) {
	ASSERT_TYPE(SELF, SnArrayType);
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
	ASSERT_TYPE(SELF, SnArrayType);
	SnArray* array = (SnArray*)SELF;
	SnString* separator = NUM_ARGS > 0 ? (SnString*)snow_call_method(ARGS[0], snow_symbol("to_string"), 0) : NULL;
	if (separator) { ASSERT_TYPE(separator, SnStringType); }
	return snow_array_join(array, separator ? snow_string_cstr(separator) : NULL);
}

SNOW_FUNC(_array_includes_p) {
	REQUIRE_ARGS(1);
	ASSERT_TYPE(SELF, SnArrayType);
	SnArray* array = (SnArray*)SELF;
	
	for (size_t i = 0; i < snow_array_size(array); ++i) {
		VALUE comparison = snow_call_method(snow_array_get(array, i), snow_symbol("="), 1, ARGS[0]);
		if (snow_eval_truth(comparison))
			return snow_int_to_value(i);
	}
	
	return SN_FALSE;
}

void SnArray_init_class(SnClass* klass)
{
	snow_define_class_method(klass, "__call__", _array_new);
	snow_define_method(klass, "get", _array_get);
	snow_define_method(klass, "[]", _array_get);
	snow_define_method(klass, "set", _array_set);
	snow_define_method(klass, "[]:", _array_set);
	snow_define_method(klass, "push", _array_push);
	snow_define_method(klass, "<<", _array_push);
	snow_define_method(klass, "pop", _array_pop);
	snow_define_method(klass, "=", _array_equals);
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
	snow_define_method(klass, "includes?", _array_includes_p);
	
	snow_define_property(klass, "length", _array_length, NULL);
	snow_define_property(klass, "any?", _array_any, NULL);
}
