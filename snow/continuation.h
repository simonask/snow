#ifndef CONTINUATION_H_C8DBOWFC
#define CONTINUATION_H_C8DBOWFC

#include "snow/object.h"
#include "snow/context.h"
#include "snow/task.h"

CAPI void snow_init_main_continuation();

CAPI SnContinuation* snow_create_continuation(SnFunctionPtr, SnContext* context);
CAPI void snow_continuation_init(SnContinuation*, SnFunctionPtr, SnContext*)         ATTR_HOT;
CAPI void snow_continuation_cleanup(SnContinuation*)                                 ATTR_HOT;
CAPI VALUE snow_continuation_call(SnContinuation*, SnContinuation* return_to)        ATTR_HOT;
#define snow_continuation_save_execution_state(CC) snow_save_execution_state(&(CC)->internal->state)
CAPI void snow_continuation_yield(SnContinuation* cc, VALUE val)                     ATTR_HOT;
CAPI void snow_continuation_return(SnContinuation* cc, VALUE val)                    ATTR_HOT;
CAPI void snow_continuation_resume(SnContinuation* cc)                               ATTR_HOT;
CAPI void snow_continuation_get_stack_bounds(SnContinuation* cc, byte** lo, byte** hi);

#endif /* end of include guard: CONTINUATION_H_C8DBOWFC */
