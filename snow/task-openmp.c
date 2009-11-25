#include "snow/task.h"
#include "snow/task-intern.h"
#include "snow/continuation.h"
#include "snow/intern.h"
#include "snow/exception-intern.h"

#include <omp.h>

static SnTask* current_tasks[SNOW_MAX_CONCURRENT_TASKS];

typedef struct SnOpenMPThreadState {
	void* stack_top;
	void* stack_bottom;
} SnOpenMPThreadState;

static SnOpenMPThreadState thread_states[SNOW_MAX_CONCURRENT_TASKS];

static void _snow_openmp_init_tasks(SnTask* tasks, size_t num_tasks)
{
	for (size_t i = 0; i < num_tasks; ++i)
	{
		tasks[i].previous = NULL;
		tasks[i].cc = NULL;
		tasks[i].exception = NULL;
		tasks[i].exception_handler = NULL;
	}
}

static void _snow_openmp_push_task(SnTask* task)
{
	ASSERT(task);
	ASSERT(task->previous == NULL);
	int id = omp_get_thread_num();
	ASSERT(id < SNOW_MAX_CONCURRENT_TASKS); // too many threads!
	task->previous = current_tasks[id];
	ASSERT(!task->previous || !task->previous->gc_barrier); // pushing tasks while gc'ing?!
	current_tasks[id] = task;
}

static SnTask* _snow_openmp_pop_task()
{
	int id = omp_get_thread_num();
	ASSERT(id < SNOW_MAX_CONCURRENT_TASKS); // too many threads!
	SnTask* task = current_tasks[id];
	current_tasks[id] = task->previous;
	return task;
}

static SnTask* _snow_openmp_top_task()
{
	int id = omp_get_thread_num();
	ASSERT(id < SNOW_MAX_CONCURRENT_TASKS); // too many threads!
	return current_tasks[id];
}

void snow_init_parallel()
{
	memset(current_tasks, 0, sizeof(SnTask*)*SNOW_MAX_CONCURRENT_TASKS);
	memset(thread_states, 0, sizeof(SnOpenMPThreadState)*SNOW_MAX_CONCURRENT_TASKS);
	static SnTask main_task;
	_snow_openmp_init_tasks(&main_task, 1);
	_snow_openmp_push_task(&main_task);
}

static void collect_exceptions_and_rethrow(SnTask* tasks, size_t num_tasks)
{
	uintx num_exceptions = 0;
	VALUE exceptions[num_tasks];
	for (int i = 0; i < num_tasks; ++i)
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

static inline void enter_thread(void* stack_top)
{
	SnOpenMPThreadState* state = &thread_states[omp_get_thread_num()];
	if (state->stack_top == NULL)
	{
		state->stack_top = stack_top;
	}
}

static inline void leave_thread(void* stack_top)
{
	SnOpenMPThreadState* state = &thread_states[omp_get_thread_num()];
	if (state->stack_top == stack_top)
	{
		state->stack_top = NULL;
	}
}

void snow_parallel_for_each(void* data, size_t element_size, size_t num_elements, SnParallelForEachCallback func, void* userdata)
{
	SnTask tasks[num_elements];
	_snow_openmp_init_tasks(tasks, num_elements);
	
	void* bottom;
	GET_STACK_PTR(bottom);
	snow_task_set_current_stack_bottom(bottom);
	
	#pragma omp parallel for
	for (int i = 0; i < num_elements; ++i)
	{
		// maintain stack extents for GC
		void* stack_top;
		GET_STACK_PTR(stack_top);
		enter_thread(stack_top);
		
		SnTask* task = &tasks[i];
		_snow_openmp_push_task(task);
		
		if (!snow_save_execution_state(&task->base))
		{
			func(data, element_size, i, userdata);
		}
		
		task = _snow_openmp_pop_task();
		ASSERT(task == &tasks[i]);
		
		// stack extents for GC
		leave_thread(stack_top);
	}
	
	collect_exceptions_and_rethrow(tasks, num_elements);
}

void snow_parallel_call_each(SnParallelCallback* funcs, size_t num, void* userdata)
{
	SnTask tasks[num];
	_snow_openmp_init_tasks(tasks, num);
	
	#pragma omp parallel for
	for (int i = 0; i < num; ++i)
	{
		// maintain stack extents for GC
		void* stack_top;
		GET_STACK_PTR(stack_top);
		enter_thread(stack_top);
		
		SnTask* task = &tasks[i];
		_snow_openmp_push_task(task);
		
		if (!snow_save_execution_state(&task->base))
		{
			funcs[i](i, userdata);
		}
		
		task = _snow_openmp_pop_task();
		ASSERT(task == &tasks[i]);
		
		// stack extents for GC
		leave_thread(stack_top);
	}

	collect_exceptions_and_rethrow(tasks, num);
}

SnTask* snow_get_current_task()
{
	return _snow_openmp_top_task();
}

uintx snow_get_current_task_id()
{
	SnTask* current_task = _snow_openmp_top_task();
	return (uintx)current_task;
}

void snow_get_current_tasks(SnTask*** out_tasks, size_t* out_num_tasks)
{
	*out_tasks = current_tasks;
	*out_num_tasks = SNOW_MAX_CONCURRENT_TASKS;
}

void snow_get_stack_extents(struct SnStackExtents extents[SNOW_MAX_CONCURRENT_TASKS])
{
	for (size_t i = 0; i < SNOW_MAX_CONCURRENT_TASKS; ++i)
	{
		extents[i].top = thread_states[i].stack_top;
		extents[i].bottom = thread_states[i].stack_bottom;
	}
}

SnExecutionState* snow_get_current_task_base()
{
	SnTask* current_task = _snow_openmp_top_task();
	ASSERT(current_task);
	return &current_task->base;
}

SnContinuation* snow_get_current_continuation()
{
	SnTask* current_task = _snow_openmp_top_task();
	ASSERT(current_task);
	return current_task->cc;
}

void snow_set_current_continuation(SnContinuation* cc)
{
	SnTask* current_task = _snow_openmp_top_task();
	ASSERT(current_task);
	ASSERT((uintx)current_task == cc->task_id);
	current_task->cc = cc;
}

VALUE snow_get_current_task_exception()
{
	SnTask* current_task = _snow_openmp_top_task();
	ASSERT(current_task);
	return current_task->exception;
}

void snow_abort_current_task(VALUE exception)
{
	SnTask* current_task = _snow_openmp_top_task();
	ASSERT(current_task);
	ASSERT(current_task->exception == NULL); // inconsistency: task should already be aborted at this point
	current_task->exception = exception;
	snow_restore_execution_state(&current_task->base);
}

SnExceptionHandler* snow_get_current_exception_handler()
{
	SnTask* current_task = _snow_openmp_top_task();
	return current_task->exception_handler;
}

void snow_set_current_exception_handler(SnExceptionHandler* handler)
{
	SnTask* current_task = _snow_openmp_top_task();
	current_task->exception_handler = handler;
}

void snow_task_set_current_stack_top(void* stack_top)
{
	SnOpenMPThreadState* state = &thread_states[omp_get_thread_num()];
	state->stack_top = stack_top;
}

void snow_task_set_current_stack_bottom(void* stack_bottom)
{
	SnOpenMPThreadState* state = &thread_states[omp_get_thread_num()];
	state->stack_bottom = stack_bottom;
}

void snow_task_departing_from_system_stack()
{
	void* here;
	GET_BASE_PTR(here);
	snow_task_set_current_stack_bottom(here);
}

void snow_task_returning_to_system_stack()
{
	snow_task_set_current_stack_bottom(NULL);
}

void snow_task_at_gc_barrier(void* stack_bottom)
{
	SnTask* task = _snow_openmp_top_task();
	task->gc_barrier = true;
	SnOpenMPThreadState* state = &thread_states[omp_get_thread_num()];
	if (state->stack_bottom == NULL)
	{
		state->stack_bottom = stack_bottom;
	}
}

void snow_task_reset_gc_barrier(void* stack_bottom)
{
	SnTask* task = _snow_openmp_top_task();
	task->gc_barrier = false;
	SnOpenMPThreadState* state = &thread_states[omp_get_thread_num()];
	if (state->stack_bottom == stack_bottom)
	{
		state->stack_bottom = NULL;
	}
}

