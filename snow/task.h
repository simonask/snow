#ifndef TASK_H_EVOMPGLT
#define TASK_H_EVOMPGLT

#include "snow/basic.h"

struct SnContext;
struct SnContinuation;

// XXX: Not an object! Should not be heap-allocated.
typedef struct SnTask
{
	struct SnContext* context;
	VALUE return_val;
	volatile bool finished;
} SnTask;

CAPI void snow_init_task_manager(uintx max_num_tasks);
CAPI void snow_task_queue(SnTask* task, struct SnContext* context);
CAPI VALUE snow_task_finish(SnTask* task);

CAPI struct SnContinuation* snow_get_current_continuation();
CAPI void snow_set_current_continuation(struct SnContinuation* cc);



#endif /* end of include guard: TASK_H_EVOMPGLT */
