#ifndef ARRAY_H_N1C9JYJW
#define ARRAY_H_N1C9JYJW

#include "snow/object.h"
#include "snow/lock.h"
#include "snow/task.h"

typedef struct SnArray {
	SnObjectBase base;
	struct array_t a;
} SnArray;

CAPI SnArray* snow_create_array();
CAPI SnArray* snow_create_array_with_size(uintx sz);
CAPI SnArray* snow_copy_array(VALUE* data, uintx len);
CAPI SnArray* snow_ref_array(VALUE* data, uintx len);
CAPI VALUE snow_array_get(SnArray*, intx);
CAPI VALUE snow_array_set(SnArray*, intx, VALUE);
CAPI VALUE snow_array_push(SnArray*, VALUE);
CAPI VALUE snow_array_pop(SnArray*);
CAPI intx snow_array_find(SnArray*, VALUE);
CAPI intx snow_array_find_or_add(SnArray*, VALUE);
CAPI void snow_array_clear(SnArray* array);
CAPI void snow_array_parallel_for_each(SnArray*, SnParallelForEachCallback func, void* userdata);

static inline VALUE* snow_array_data(SnArray* ar) { return ar->a.data; }
static inline uintx snow_array_size(SnArray* ar) { return ar->a.size; }

#endif /* end of include guard: ARRAY_H_N1C9JYJW */
