#ifndef GC_H_3BT7YTTZ
#define GC_H_3BT7YTTZ

#include "snow/basic.h"
#include "snow/object.h"

#define ATTR_ALLOC_SIZE(...) __attribute__((alloc_size((__VA_ARGS__))))
#define ATTR_MALLOC __attribute__((malloc))

typedef void(*SnGCFreeFunc)(VALUE val);

CAPI SnObjectBase* snow_gc_alloc_object(uintx size)           ATTR_ALLOC_SIZE(1) ATTR_MALLOC;
CAPI void* snow_gc_alloc(uintx size)                          ATTR_ALLOC_SIZE(1) ATTR_MALLOC;
CAPI void snow_gc_set_free_func(void* data, SnGCFreeFunc);
CAPI void snow_gc();
CAPI void snow_gc_stack_top(void*);

#endif /* end of include guard: GC_H_3BT7YTTZ */
