#include "snow/task.h"
#include "snow/context.h"
#include "snow/function.h"
#include "snow/intern.h"
#include "snow/task-intern.h"
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

void snow_init_task_manager(uintx max_num_threads)
{
	_init_task_manager();
}

SnContinuation* snow_get_current_continuation()
{
	return _get_current_continuation();
}

void snow_set_current_continuation(SnContinuation* cc)
{
	_set_current_continuation(cc);
}

void snow_task_queue(SnTask* task, SnContext* context)
{
	task->context = context;
	task->return_val = NULL;
	task->finished = false;
	_task_queue(task, context);
}

VALUE snow_task_finish(SnTask* task)
{
	_task_finish(task);
	return task->return_val;
}

