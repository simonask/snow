#ifndef GC_H_3BT7YTTZ
#define GC_H_3BT7YTTZ

#include "snow/basic.h"
#include "snow/object.h"
#include "snow/arch.h"
#include "snow/task.h"

static const int SNOW_GC_ALIGNMENT = 0x10;
static inline uintx snow_round_to_alignment(uintx size, uintx alignment) { return size + ((alignment - (size % alignment)) % alignment); }

typedef void(*SnGCFreeFunc)(VALUE val);

/*
	snow_init_gc: Initialize GC structures.
*/
CAPI void snow_init_gc();

/*
	snow_gc_alloc_object: allocates and initializes a Snow object of the given type.
*/
CAPI SnAnyObject* snow_gc_alloc_object(SnValueType type);

/*
	snow_gc: Force GC invocation.
*/
CAPI void snow_gc();

/*
	snow_gc_allocated_size: Return the number of bytes allocated for the pointer. Might be larger than
	the original number of bytes requested for the allocation.
*/
CAPI uintx snow_gc_allocated_size(const void* data);

extern volatile bool _snow_gc_is_collecting;
static inline bool snow_gc_is_collecting() { return _snow_gc_is_collecting; }

/*
	snow_gc_barrier: Insert GC barrier, which gives the GC a chance to temporarily stop the thread
	while performing a collection. Insert calls to this function in busy loops and functions that
	are called extraordinarily often to allow maximum parallellism.
*/
CAPI void snow_gc_barrier();

// Use these functions to let the GC know about memory that might contain pointers to Snow objects.
CAPI void* snow_malloc_gc_root(uintx size)                           ATTR_ALLOC_SIZE(1);
CAPI void* snow_calloc_gc_root(uintx count, uintx size)              ATTR_ALLOC_SIZE(2);
CAPI void* snow_realloc_gc_root(void* ptr, uintx new_size)           ATTR_ALLOC_SIZE(1);
CAPI void snow_free_gc_root(void* ptr);
// Use these functions for memory that you don't control the allocation of, but might still
// contain GC roots.
CAPI void snow_gc_add_root(void* ptr, uintx size);
CAPI void snow_gc_remove_root(void* ptr);

#endif /* end of include guard: GC_H_3BT7YTTZ */
