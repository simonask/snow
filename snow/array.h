#ifndef ARRAY_H_N1C9JYJW
#define ARRAY_H_N1C9JYJW

#include "snow/object.h"
#include "snow/lock-impl.h"
#include "snow/task.h"

CAPI SnArray* snow_create_array();
CAPI SnArray* snow_create_array_with_size(uintx sz);
CAPI SnArray* snow_create_fixed_array(uintx sz);
CAPI SnArray* snow_copy_array(VALUE* data, uintx len);
CAPI VALUE snow_array_get(SnArray*, intx);
CAPI VALUE snow_array_set(SnArray*, intx, VALUE);
CAPI VALUE snow_array_erase(SnArray*, intx idx);
CAPI VALUE snow_array_push(SnArray*, VALUE);
CAPI VALUE snow_array_pop(SnArray*);
CAPI intx snow_array_find(SnArray*, VALUE);
CAPI intx snow_array_find_or_add(SnArray*, VALUE);
CAPI void snow_array_clear(SnArray* array);
CAPI struct SnString* snow_array_join(SnArray* array, const char* sep);
CAPI void snow_array_parallel_for_each(SnArray*, SnParallelForEachCallback func, void* userdata);

static inline VALUE* snow_array_data(SnArray* ar) { return ar->data; }
static inline uintx snow_array_size(SnArray* ar) { return ar->size; }

#endif /* end of include guard: ARRAY_H_N1C9JYJW */
