#ifndef GC_H_3BT7YTTZ
#define GC_H_3BT7YTTZ

#include "snow/basic.h"
#include "snow/object.h"

typedef void(*SnGCFreeFunc)(VALUE val);

CAPI SnObjectBase* snow_gc_alloc_object(uintx size);
CAPI void* snow_gc_alloc(uintx size);
CAPI void snow_gc_set_free_func(void* data, SnGCFreeFunc);
CAPI void snow_gc();
CAPI void snow_gc_stack_top(void*);

#endif /* end of include guard: GC_H_3BT7YTTZ */
