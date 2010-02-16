#ifndef TASK_H_EVOMPGLT
#define TASK_H_EVOMPGLT

#include "snow/basic.h"
#include "snow/object.h"
#include <unistd.h>

CAPI void snow_init_parallel(void* stack_top);

typedef void(*SnParallelForEachCallback)(void* data, size_t element_size, size_t index, void* userdata);
CAPI void snow_parallel_for_each(void* data, size_t element_size, size_t num_elements, SnParallelForEachCallback func, void* userdata);

struct SnTask;
typedef struct SnDeferredTask {
	SnObjectBase base;
	VALUE closure;
	VALUE result;
	
	void* private;
	struct SnTask* task;
} SnDeferredTask;

CAPI SnDeferredTask* snow_deferred_call(VALUE closure);
CAPI VALUE snow_deferred_task_wait(SnDeferredTask* task);

/*
	snow_gc_barrier_enter: Tell the GC that it is safe to perform a collection between this call and a
	call to snow_gc_barrier_leave.
	
	CAUTION: Accessing any GC-allocated memory between snow_gc_barrier_enter() and
	snow_gc_barrier_leave() will cause undefined behavior. Calling snow_gc_barrier_enter() without
	calling snow_gc_barrier_leave() will cause undefined behavior. Calling snow_gc_barrier_leave()
	without having called snow_gc_barrier_enter() beforehand will cause undefined behavior.
*/
CAPI void snow_gc_barrier_enter();

/*
	snow_gc_barrier_leave: Tell the GC that it is no longer safe to perform a collection. If a
	collection is in progress, this thread will wait until the collection is done before returning
	from snow_gc_barrier_leave.
	
	See snow_gc_barrier_enter for warnings and cautions about this.
*/
CAPI void snow_gc_barrier_leave();

CAPI uintx snow_get_current_task_id();
static inline void snow_task_yield() { /*usleep(1);*/ /* pthread_yield? */ }

struct SnContinuation;
CAPI struct SnContinuation* snow_get_current_continuation();

#endif /* end of include guard: TASK_H_EVOMPGLT */
