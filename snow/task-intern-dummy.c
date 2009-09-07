#include "snow/task-intern.h"
#include "snow/intern.h"
#include "snow/context.h"
#include "snow/function.h"
#include <pthread.h>

static pthread_key_t _current_continuation_key;

void _init_task_manager()
{
	int r;
	
	r = pthread_key_create(&_current_continuation_key, NULL);
	ASSERT(r == 0);
}

void _set_current_continuation(struct SnContinuation* cc)
{
	int r;
	r = pthread_setspecific(_current_continuation_key, cc);
	ASSERT(r == 0);
}

struct SnContinuation* _get_current_continuation()
{
	void* cc = pthread_getspecific(_current_continuation_key);
	return (struct SnContinuation*)cc;
}

void _task_queue(SnTask* task, SnContext* context)
{
	task->return_val = snow_function_call(context->function, context);
	task->finished = true;
}

void _task_finish(SnTask* task)
{
	ASSERT(task->finished);
}
