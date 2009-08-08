#ifndef ARRAY_INTERN_H_JUS95Q03
#define ARRAY_INTERN_H_JUS95Q03

#include "snow/basic.h"
#include "snow/gc.h"
#include "snow/intern.h"

static inline void array_init(struct array_t* array)
{
	array->data = 0;
	array->size = 0;
	array->alloc_size = 0;
}

static inline void array_init_with_size(struct array_t* array, uintx size)
{
	array->data = (VALUE*)snow_gc_alloc(size * sizeof(VALUE));
	array->size = 0;
	array->alloc_size = size;
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

#endif /* end of include guard: ARRAY_INTERN_H_JUS95Q03 */
