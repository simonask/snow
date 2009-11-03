#ifndef TASK_INTERN_H_VG31U3ZY
#define TASK_INTERN_H_VG31U3ZY

#include "snow/task.h"

struct SnExceptionHandler;

CAPI struct SnExecutionState* snow_get_current_task_base();
CAPI void snow_set_current_continuation(struct SnContinuation* cc);
CAPI void snow_abort_current_task(VALUE exception);
CAPI VALUE snow_get_current_task_exception();
CAPI struct SnExceptionHandler* snow_get_current_exception_handler();
CAPI void snow_set_current_exception_handler(struct SnExceptionHandler* handler);

#endif /* end of include guard: TASK_INTERN_H_VG31U3ZY */
