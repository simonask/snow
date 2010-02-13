#ifndef TASK_INTERN_H_VG31U3ZY
#define TASK_INTERN_H_VG31U3ZY

#include "snow/task.h"
#include "snow/arch.h"

struct SnExceptionHandler;
struct SnContinuation;

typedef struct SnTask {
	struct SnTask* previous;
	struct SnContinuation* continuation;
	VALUE exception;
	struct SnExceptionHandler* exception_handler;
	struct SnContinuation* base; // catch-all for exceptions
	void* stack_top;
	void* stack_bottom;
} SnTask;

CAPI SnTask* snow_get_current_task();
typedef void(*SnTaskIteratorFunc)(SnTask* task, void* userdata);
CAPI void snow_with_each_task_do(SnTaskIteratorFunc func, void* userdata);

CAPI void snow_set_gc_barriers();   // locks all gc barriers across all threads
CAPI void snow_unset_gc_barriers(); // unlocks all gc barriers

CAPI void snow_set_current_continuation(struct SnContinuation* cc);
CAPI void snow_abort_current_task(VALUE exception);
static inline bool snow_task_is_base_continuation(struct SnContinuation* cc) { return snow_get_current_task()->base == cc; }

CAPI struct SnExceptionHandler* snow_current_exception_handler();
CAPI void snow_push_exception_handler(struct SnExceptionHandler*);
CAPI void snow_pop_exception_handler();

#endif /* end of include guard: TASK_INTERN_H_VG31U3ZY */
