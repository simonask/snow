#ifndef GC_H_3BT7YTTZ
#define GC_H_3BT7YTTZ

#include "snow/basic.h"
#include "snow/object.h"

#if defined(__GNU_C__) && GCC_VERSION >= MAKE_VERSION(4, 4, 0)
#define ATTR_ALLOC_SIZE(...) __attribute__((alloc_size((__VA_ARGS__))))
#define ATTR_MALLOC __attribute__((malloc))
#else
#define ATTR_ALLOC_SIZE(...)
#define ATTR_MALLOC
#endif

static const int SNOW_GC_ALIGNMENT = 0x10;
static inline uintx snow_gc_round(uintx size) { return size + ((SNOW_GC_ALIGNMENT - (size % SNOW_GC_ALIGNMENT)) % SNOW_GC_ALIGNMENT); }

typedef void(*SnGCFreeFunc)(VALUE val);

CAPI SnObjectBase* snow_gc_alloc_object(uintx size)           ATTR_ALLOC_SIZE(1) ATTR_MALLOC;
CAPI void* snow_gc_alloc(uintx size)                          ATTR_ALLOC_SIZE(1) ATTR_MALLOC;
CAPI void snow_gc_set_free_func(void* data, SnGCFreeFunc);
CAPI void snow_gc();
CAPI void snow_gc_barrier();
CAPI uintx snow_gc_allocated_size(void* data);

CAPI void* snow_malloc(uintx size);
CAPI void* snow_calloc(uintx count, uintx size);
CAPI void* snow_realloc(void* ptr, uintx new_size);
CAPI void* snow_malloc_aligned(uintx size, uintx alignment);
CAPI void snow_free(void* ptr);

#endif /* end of include guard: GC_H_3BT7YTTZ */
