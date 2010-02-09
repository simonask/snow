#ifndef GC_H_3BT7YTTZ
#define GC_H_3BT7YTTZ

#include "snow/basic.h"
#include "snow/object.h"
#include "snow/arch.h"
#include "snow/task.h"

#if defined(__GNU_C__) && GCC_VERSION >= MAKE_VERSION(4, 4, 0)
#define ATTR_ALLOC_SIZE(...) __attribute__((alloc_size((__VA_ARGS__)),malloc))
#else
#define ATTR_ALLOC_SIZE(...)
#define ATTR_MALLOC
#endif

static const int SNOW_GC_ALIGNMENT = 0x10;
static inline uintx snow_gc_round(uintx size) { return size + ((SNOW_GC_ALIGNMENT - (size % SNOW_GC_ALIGNMENT)) % SNOW_GC_ALIGNMENT); }

typedef void(*SnGCFreeFunc)(VALUE val);

/*
	snow_init_gc: Initialize GC structures.
*/
CAPI void snow_init_gc();

/*
	snow_gc_alloc_object: allocates memory for use with anything that derives from SnObjectBase.
	The `type'-field of SnObjectBase is used to recognize roots within the object, so it is important
	that this function is not used for anything by Snow objects.
*/
CAPI SnObjectBase* snow_gc_alloc_object(uintx size) ATTR_ALLOC_SIZE(1);

/*
	snow_gc_alloc_blob: allocates memory that doesn't contain a specific struct, but can contain roots.
	During GC, the memory will be conservatively scanned for roots. This is mostly useful for various
	types of lists of VALUEs. Remember to NULL erased elements to avoid false roots.
*/
CAPI VALUE* snow_gc_alloc_blob(uintx size) ATTR_ALLOC_SIZE(1);

/*
	snow_gc_alloc_atomic: allocates memory that will not contain roots. Memory allocated this way will
	not be scanned for pointers to other objects. This is mostly useful for arrays of simple types, such
	as char* and int*.
*/
CAPI void* snow_gc_alloc_atomic(uintx size) ATTR_ALLOC_SIZE(1);

/*
	snow_gc_realloc: Reallocates memory that is already GC-allocated. The semantics are the same as the
	system-provided realloc(3).
*/
CAPI void* snow_gc_realloc(void* ptr, uintx size) ATTR_ALLOC_SIZE(1);

/*
	snow_gc_set_free_func: sets a finalizer function for the given pointer, which will be called when
	the memory pointed to by that pointer is garbage collected.
*/
CAPI void snow_gc_set_free_func(const void* data, SnGCFreeFunc);

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

CAPI bool _snow_gc_task_at_barrier();
CAPI void _snow_gc_task_set_barrier();
CAPI void _snow_gc_task_unset_barrier();

/*
	snow_gc_barrier_enter: Tell the GC that it is safe to perform a collection between this call and a
	call to snow_gc_barrier_leave.
	
	CAUTION: Accessing any GC-allocated memory between snow_gc_barrier_enter() and
	snow_gc_barrier_leave() will cause undefined behavior. Calling snow_gc_barrier_enter() without
	calling snow_gc_barrier_leave() will cause undefined behavior. Calling snow_gc_barrier_leave()
	without having called snow_gc_barrier_enter() beforehand will cause undefined behavior.
*/
static inline void snow_gc_barrier_enter() {
	if (snow_gc_is_collecting()) _snow_gc_task_set_barrier();
}

/*
	snow_gc_barrier_leave: Tell the GC that it is no longer safe to perform a collection. If a
	collection is in progress, this thread will wait until the collection is done before returning
	from snow_gc_barrier_leave.
	
	See snow_gc_barrier_enter for warnings and cautions about this.
*/
static inline void snow_gc_barrier_leave() {
	while (snow_gc_is_collecting() && _snow_gc_task_at_barrier()) snow_task_yield();
	_snow_gc_task_unset_barrier();
}

/*
	snow_gc_barrier: Insert GC barrier, which gives the GC a chance to temporarily stop the thread
	while performing a collection. Insert calls to this function in busy loops and functions that
	are called extraordinarily often to allow maximum parallellism.
*/
static inline void snow_gc_barrier() {
	if (snow_gc_is_collecting()) {
		SnExecutionState state;
		if (snow_save_execution_state(&state))
			return;
		_snow_gc_task_set_barrier();
		while (snow_gc_is_collecting()) snow_task_yield();
		_snow_gc_task_unset_barrier();
		snow_restore_execution_state(&state);
	}
}

// Snow Malloc Interface
CAPI void* snow_malloc(uintx size)                                             ATTR_ALLOC_SIZE(1);
CAPI void* snow_calloc(uintx count, uintx size)                                ATTR_ALLOC_SIZE(2);
CAPI void* snow_realloc(void* ptr, uintx new_size)                             ATTR_ALLOC_SIZE(2);
CAPI void* snow_malloc_aligned(uintx size, uintx alignment)                    ATTR_ALLOC_SIZE(1);
CAPI void snow_free(void* ptr);

#endif /* end of include guard: GC_H_3BT7YTTZ */
