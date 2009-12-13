#ifndef ARRAY_INTERN_H_JUS95Q03
#define ARRAY_INTERN_H_JUS95Q03

#include "snow/basic.h"
#include "snow/gc.h"
#include "snow/intern.h"

typedef intx(*compare_func_t)(VALUE a, VALUE b);

static inline void array_init(struct array_t* array)
{
	array->data = 0;
	array->size = 0;
	array->alloc_size = 0;
}

static inline void array_init_with_size(struct array_t* array, uintx size)
{
	array->data = size ? (VALUE*)snow_gc_alloc(size * sizeof(VALUE)) : NULL;
	array->size = 0;
	array->alloc_size = size;
}

static inline uintx array_size(struct array_t* array)
{
	return array->size;
}

static inline void array_grow(struct array_t* array, intx new_size)
{
	if (new_size > array->alloc_size)
	{
		VALUE* new_data = (VALUE*)snow_gc_alloc(sizeof(VALUE) * new_size);
		memcpy(new_data, array->data, sizeof(VALUE) * array->size);
		array->data = new_data;
		array->alloc_size = new_size;
	}
	
	if (new_size > array->size)
	{
		for (intx i = array->size; i < new_size; ++i)
			array->data[i] = SN_NIL;
		array->size = new_size;
	}
}

static inline VALUE array_get(struct array_t* array, intx idx)
{
	// WARNING: This is explicitly inlined in codegens. If you change this, you must also change each codegen that inlines this.
	if (idx >= array->size)
		return NULL;
	if (idx < 0)
		idx += array->size;
	if (idx < 0)
		return NULL;
	return array->data[idx];
}

static inline VALUE array_set(struct array_t* array, intx idx, VALUE val)
{
	if (idx >= array->size)
		array_grow(array, idx+1);
	if (idx < 0)
		idx += array->size;
	if (idx < 0)
		TRAP(); // index out of bounds
	array->data[idx] = val;
	return val;
}

static inline VALUE array_push(struct array_t* array, VALUE val)
{
	return array_set(array, array_size(array), val);
}

static inline VALUE array_pop(struct array_t* array)
{
	if (array->size)
	{
		VALUE val = array->data[--array->size];
		array->data[array->size] = SN_NIL; // for gc
		return val;
	}
	else
		return NULL;
}

static inline intx array_find(struct array_t* array, VALUE val)
{
	for (intx i = 0; i < array->size; ++i)
	{
		if (array->data[i] == val)
			return i;
	}
	return -1;
}

static inline intx array_find_with_compare(struct array_t* array, VALUE val, compare_func_t cmp)
{
	for (intx i = 0; i < array->size; ++i)
	{
		if (cmp(array->data[i], val) == 0)
			return i;
	}
	return -1;
}

static inline intx array_find_or_add(struct array_t* array, VALUE val)
{
	int i = array_find(array, val);
	if (i >= 0) return i;
	i = array_size(array);
	array_push(array, val);
	return i;
}

#endif /* end of include guard: ARRAY_INTERN_H_JUS95Q03 */
