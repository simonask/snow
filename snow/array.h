#ifndef ARRAY_H_N1C9JYJW
#define ARRAY_H_N1C9JYJW

#include "snow/object.h"

typedef struct SnArray {
	SnObject base;
	uintx alloc_size;
	uintx size;
	VALUE* data;
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


static inline uintx snow_array_size(SnArray* ar) { return ar->size; }

#endif /* end of include guard: ARRAY_H_N1C9JYJW */
