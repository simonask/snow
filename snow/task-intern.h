#ifndef TASK_INTERN_H_VG31U3ZY
#define TASK_INTERN_H_VG31U3ZY

#include "snow/task.h"
#include "snow/arch.h"

#define SNOW_MAX_CONCURRENT_TASKS 64 // should be good enough for at least 4-5 years

struct SnExceptionHandler;
struct SnContinuation;

typedef struct SnTask {
	struct SnTask* previous;
	struct SnContinuation* continuation;
	VALUE exception;
	struct SnExceptionHandler* exception_handler;
	struct SnContinuation* base; // catch-all for exceptions
	bool started_on_system_stack;
} SnTask;

CAPI SnTask* snow_get_current_task();

CAPI uintx snow_get_current_thread_index();
CAPI void snow_get_thread_stack_extents(size_t thread_index, void** top, void** bottom);
CAPI bool snow_thread_is_at_gc_barrier(size_t thread_index);
CAPI void snow_thread_set_gc_barrier(size_t thread_index);
CAPI void snow_thread_unset_gc_barrier(size_t thread_index);
CAPI bool snow_all_threads_at_gc_barrier();

CAPI void snow_get_top_tasks(SnTask** out_tasks, size_t* out_num_threads);
CAPI void snow_set_current_continuation(struct SnContinuation* cc);
CAPI void snow_abort_current_task(VALUE exception);
static inline bool snow_task_is_base_continuation(struct SnContinuation* cc) { return snow_get_current_task()->base == cc; }

CAPI bool snow_thread_is_on_system_stack();
CAPI void snow_thread_departing_from_system_stack();
CAPI void snow_thread_returning_to_system_stack();

#endif /* end of include guard: TASK_INTERN_H_VG31U3ZY */
