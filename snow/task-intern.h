#ifndef TASK_INTERN_H_TV0H2ZC1
#define TASK_INTERN_H_TV0H2ZC1

#include "snow/basic.h"
#include "snow/task.h"

/*
	These functions must be implemented by any task-intern-*.c implementation.
	(task-intern-pthreads.c, task-intern-dispatch.c, etc.)
*/

HIDDEN void _init_task_manager();
HIDDEN void _task_queue(SnTask* task, struct SnContext* context);
HIDDEN void _task_finish(SnTask* task);

HIDDEN struct SnContinuation* _get_current_continuation();
HIDDEN void _set_current_continuation(struct SnContinuation*);

#endif /* end of include guard: TASK_INTERN_H_TV0H2ZC1 */
