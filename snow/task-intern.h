#ifndef TASK_INTERN_H_VG31U3ZY
#define TASK_INTERN_H_VG31U3ZY

#include "snow/task.h"
#include "snow/arch.h"

#define SNOW_MAX_CONCURRENT_TASKS 64 // should be good enough for at least 4-5 years

struct SnExceptionHandler;
struct SnContinuation;

typedef struct SnTask {
	struct SnTask* previous;
	struct SnContinuation* cc;
	VALUE exception;
	struct SnExceptionHandler* exception_handler;
	SnExecutionState base; // used as a catch-all for propagating exceptions to parent task
	volatile bool gc_barrier;
} SnTask;

CAPI SnTask* snow_get_current_task();
CAPI void snow_get_current_tasks(SnTask*** out_tasks, size_t* out_num_tasks);

struct SnStackExtents { void* top; void* bottom; };
CAPI void snow_get_stack_extents(struct SnStackExtents extents[SNOW_MAX_CONCURRENT_TASKS]);

CAPI struct SnExecutionState* snow_get_current_task_base();
CAPI void snow_set_current_continuation(struct SnContinuation* cc);
CAPI void snow_abort_current_task(VALUE exception);
CAPI VALUE snow_get_current_task_exception();
CAPI struct SnExceptionHandler* snow_get_current_exception_handler();
CAPI void snow_set_current_exception_handler(struct SnExceptionHandler* handler);

CAPI void snow_task_set_current_stack_bottom(void* bottom);
CAPI void snow_task_set_current_stack_top(void* top);
CAPI void snow_task_departing_from_system_stack();
CAPI void snow_task_returning_to_system_stack();
CAPI void snow_task_at_gc_barrier(void* stack_bottom);
CAPI void snow_task_reset_gc_barrier(void* stack_bottom);

#endif /* end of include guard: TASK_INTERN_H_VG31U3ZY */
