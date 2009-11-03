#include "snow/task.h"
#include "snow/task-intern.h"
#include "snow/continuation.h"
#include "snow/intern.h"
#include "snow/exception-intern.h"

#include <omp.h>

#define MAX_PARALLEL_TASKS 64 // should be good enough for at least 4-5 years

typedef struct SnOpenMPTask {
	struct SnOpenMPTask* previous;
	SnContinuation* cc;
	VALUE exception;
	SnExceptionHandler* exception_handler;
	SnExecutionState base; // used as a catch-all for propagating exceptions to parent task
} SnOpenMPTask;

static SnOpenMPTask* current_tasks[MAX_PARALLEL_TASKS];

static void _snow_openmp_init_tasks(SnOpenMPTask* tasks, size_t num_tasks)
{
	for (size_t i = 0; i < num_tasks; ++i)
	{
		tasks[i].previous = NULL;
		tasks[i].cc = NULL;
		tasks[i].exception = NULL;
		tasks[i].exception_handler = NULL;
	}
}

static void _snow_openmp_push_task(SnOpenMPTask* task)
{
	ASSERT(task);
	ASSERT(task->previous == NULL);
	int id = omp_get_thread_num();
	ASSERT(id < MAX_PARALLEL_TASKS); // too many threads!
	task->previous = current_tasks[id];
	current_tasks[id] = task;
}

static SnOpenMPTask* _snow_openmp_pop_task()
{
	int id = omp_get_thread_num();
	ASSERT(id < MAX_PARALLEL_TASKS); // too many threads!
	SnOpenMPTask* task = current_tasks[id];
	current_tasks[id] = task->previous;
	return task;
}

static SnOpenMPTask* _snow_openmp_top_task()
{
	int id = omp_get_thread_num();
	ASSERT(id < MAX_PARALLEL_TASKS); // too many threads!
	return current_tasks[id];
}

void snow_init_parallel()
{
	memset(current_tasks, 0, sizeof(SnOpenMPTask*)*MAX_PARALLEL_TASKS);
	SnOpenMPTask* main_task = (SnOpenMPTask*)snow_malloc(sizeof(SnOpenMPTask));
	_snow_openmp_init_tasks(main_task, 1);
	_snow_openmp_push_task(main_task);
}

void snow_parallel_for_each(void* data, size_t element_size, size_t num_elements, SnParallelForEachCallback func, void* userdata)
{
	SnOpenMPTask tasks[num_elements];
	_snow_openmp_init_tasks(tasks, num_elements);
	
	#pragma omp parallel for
	for (int i = 0; i < num_elements; ++i)
	{
		SnOpenMPTask* task = &tasks[i];
		_snow_openmp_push_task(task);
		
		if (!snow_save_execution_state(&task->base))
		{
			func(data, element_size, i, userdata);
		}
		
		task = _snow_openmp_pop_task();
		ASSERT(task == &tasks[i]);
	}
	
	uintx num_exceptions = 0;
	VALUE exceptions[num_elements];
	for (int i = 0; i < num_elements; ++i)
	{
		if (tasks[i].exception) exceptions[num_exceptions++] = tasks[i].exception;
	}
	if (num_exceptions) {
		VALUE rethrow = NULL;
		if (num_exceptions == 1)
			rethrow = exceptions[0];
		else
			rethrow = snow_copy_array(exceptions, num_exceptions);
		snow_throw_exception(rethrow);
	}
}

void snow_parallel_call_each(SnParallelCallback* funcs, size_t num, void* userdata)
{
	SnOpenMPTask tasks[num];
	_snow_openmp_init_tasks(tasks, num);
	
	#pragma omp parallel for
	for (int i = 0; i < num; ++i)
	{
		SnOpenMPTask* task = &tasks[i];
		_snow_openmp_push_task(task);
		
		if (!snow_save_execution_state(&task->base))
		{
			funcs[i](i, userdata);
		}
		
		task = _snow_openmp_pop_task();
		ASSERT(task == &tasks[i]);
	}
	
	uintx num_exceptions = 0;
	VALUE exceptions[num];
	for (int i = 0; i < num; ++i)
	{
		if (tasks[i].exception) exceptions[num_exceptions++] = tasks[i].exception;
	}
	if (num_exceptions) {
		VALUE rethrow = NULL;
		if (num_exceptions == 1)
			rethrow = exceptions[0];
		else
			rethrow = snow_copy_array(exceptions, num_exceptions);
		snow_throw_exception(rethrow);
	}
}

uintx snow_get_current_task_id()
{
	SnOpenMPTask* current_task = _snow_openmp_top_task();
	return (uintx)current_task;
}

SnExecutionState* snow_get_current_task_base()
{
	SnOpenMPTask* current_task = _snow_openmp_top_task();
	ASSERT(current_task);
	return &current_task->base;
}

SnContinuation* snow_get_current_continuation()
{
	SnOpenMPTask* current_task = _snow_openmp_top_task();
	ASSERT(current_task);
	return current_task->cc;
}

void snow_set_current_continuation(SnContinuation* cc)
{
	SnOpenMPTask* current_task = _snow_openmp_top_task();
	ASSERT(current_task);
	current_task->cc = cc;
}

VALUE snow_get_current_task_exception()
{
	SnOpenMPTask* current_task = _snow_openmp_top_task();
	ASSERT(current_task);
	return current_task->exception;
}

void snow_abort_current_task(VALUE exception)
{
	SnOpenMPTask* current_task = _snow_openmp_top_task();
	ASSERT(current_task);
	ASSERT(current_task->exception == NULL); // inconsistency: task should already be aborted at this point
	current_task->exception = exception;
	snow_restore_execution_state(&current_task->base);
}

SnExceptionHandler* snow_get_current_exception_handler()
{
	SnOpenMPTask* current_task = _snow_openmp_top_task();
	return current_task->exception_handler;
}

void snow_set_current_exception_handler(SnExceptionHandler* handler)
{
	SnOpenMPTask* current_task = _snow_openmp_top_task();
	current_task->exception_handler = handler;
}
