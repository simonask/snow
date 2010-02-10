#include "snow/task.h"
#include "snow/task-intern.h"
#include "snow/continuation.h"
#include "snow/intern.h"
#include "snow/exception-intern.h"

#include <omp.h>

typedef struct SnOpenMPThreadState {
	SnTask* task;
	volatile bool gc_barrier;
	void* stack_top;
	void* stack_bottom;
} SnOpenMPThreadState;

static SnOpenMPThreadState threads[SNOW_MAX_CONCURRENT_TASKS];

static void _snow_openmp_init_threads();
static void _snow_openmp_push_task(SnTask* task, void* stack_top);
static SnTask* _snow_openmp_pop_task();

static int PURE me() { return omp_get_thread_num(); }
#define ME (me())

void snow_init_parallel(void* stack_top)
{
	memset(threads, 0, SNOW_MAX_CONCURRENT_TASKS * sizeof(SnOpenMPThreadState));
	static SnTask main_task;
	memset(&main_task, 0, sizeof(SnTask));
	_snow_openmp_push_task(&main_task, stack_top);
}

static void _snow_openmp_push_task(SnTask* task, void* stack_top)
{
	ASSERT(task);
	ASSERT(task->previous == NULL);
	ASSERT(ME < SNOW_MAX_CONCURRENT_TASKS); // too many threads!
	task->previous = threads[ME].task;
	ASSERT(!task->previous || !threads[ME].gc_barrier); // pushing tasks while gc'ing?!
	threads[ME].task = task;
	if (threads[ME].stack_top == NULL) {
		threads[ME].stack_top = stack_top;
	}
}

static SnTask* _snow_openmp_pop_task()
{
	ASSERT(ME < SNOW_MAX_CONCURRENT_TASKS); // too many threads! also, how is this possible?
	ASSERT(!threads[ME].gc_barrier); // finishing task while gc'ing?!
	SnTask* task = threads[ME].task;
	threads[ME].task = task->previous;
	if (threads[ME].task == NULL) {
		// no more tasks in thread
		threads[ME].stack_top = NULL;
	}
	return task;
}

static SnTask* _snow_openmp_top_task()
{
	ASSERT(ME < SNOW_MAX_CONCURRENT_TASKS); // too many threads!
	return threads[ME].task;
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


void snow_parallel_for_each(void* data, size_t element_size, size_t num_elements, SnParallelForEachCallback func, void* userdata)
{
	SnTask tasks[num_elements];
	memset(tasks, 0, sizeof(tasks));
	
	#pragma omp parallel for
	for (int i = 0; i < num_elements; ++i)
	{
		SnTask* task = &tasks[i];
		
		SnContinuation base;
		snow_continuation_init(&base, NULL, NULL);
		
		task->base = &base;
		task->base->task_id = (uintx)task;
		task->base->running = true;
		task->continuation = task->base;
		task->started_on_system_stack = snow_thread_is_on_system_stack();
		
		void* stack_top;
		GET_STACK_PTR(stack_top);
		_snow_openmp_push_task(task, stack_top);
		
		if (!snow_continuation_save_execution_state(task->base))
		{
			func(data, element_size, i, userdata);
		}
		
		task = _snow_openmp_pop_task();
		ASSERT(task == &tasks[i]);
	}
	
	collect_exceptions_and_rethrow(tasks, num_elements);
}

SnTask* snow_get_current_task()
{
	return _snow_openmp_top_task();
}

uintx snow_get_current_thread_index()
{
	return ME;
}

uintx snow_get_current_task_id()
{
	SnTask* current_task = _snow_openmp_top_task();
	return (uintx)current_task;
}

void snow_get_thread_stack_extents(size_t thread_index, void** top, void** bottom)
{
	ASSERT(thread_index < SNOW_MAX_CONCURRENT_TASKS);
	SnOpenMPThreadState* thread = &threads[thread_index];
	*top = thread->stack_top;
	*bottom = thread->stack_bottom;
}

bool snow_thread_is_at_gc_barrier(size_t thread_index)
{
	ASSERT(thread_index < SNOW_MAX_CONCURRENT_TASKS);
	return threads[thread_index].gc_barrier;
}

void snow_thread_set_gc_barrier(size_t thread_index)
{
	threads[thread_index].gc_barrier = true;
}

void snow_thread_unset_gc_barrier(size_t thread_index)
{
	threads[thread_index].gc_barrier = false;
}

bool snow_all_threads_at_gc_barrier() {
	ASSERT(snow_gc_is_collecting());
	ASSERT(threads[ME].gc_barrier);
	for (size_t i = 0; i < SNOW_MAX_CONCURRENT_TASKS; ++i) {
		SnOpenMPThreadState* thread = &threads[i];
		if (thread->task && !thread->gc_barrier) return false;
	}
	return true;
}

void snow_get_top_tasks(SnTask** out_tasks, size_t* out_num_threads)
{
	*out_num_threads = 0;
	for (size_t i = 0; i < SNOW_MAX_CONCURRENT_TASKS; ++i) {
		if (threads[i].task) {
			out_tasks[(*out_num_threads)++] = threads[i].task;
		}
	}
}

SnContinuation* snow_get_current_continuation()
{
	return threads[ME].task->continuation;
}

void snow_set_current_continuation(SnContinuation* cc)
{
	SnTask* current_task = threads[ME].task;
	ASSERT((uintx)current_task == cc->task_id);
	current_task->continuation = cc;
}

void snow_abort_current_task(VALUE exception)
{
	SnTask* current_task = _snow_openmp_top_task();
	ASSERT(current_task);
	ASSERT(current_task->exception == NULL); // inconsistency: task should already be aborted at this point
	current_task->exception = exception;
	snow_continuation_resume(current_task->base);
}

bool snow_thread_is_on_system_stack()
{
	return threads[ME].stack_bottom == NULL;
}

void snow_thread_departing_from_system_stack(void* stack_bottom)
{
	ASSERT(threads[ME].stack_bottom == NULL);
	if (threads[ME].stack_bottom == NULL)
		threads[ME].stack_bottom = stack_bottom;
}

void snow_thread_returning_to_system_stack()
{
	SnTask* task = snow_get_current_task();
	if (threads[ME].stack_bottom != NULL)
		threads[ME].stack_bottom = NULL;
}

SnExceptionHandler* snow_current_exception_handler()
{
	return snow_get_current_task()->exception_handler;
}

void snow_push_exception_handler(SnExceptionHandler* handler)
{
	handler->previous = snow_current_exception_handler();
	snow_get_current_task()->exception_handler = handler;
}

void snow_pop_exception_handler()
{
	SnTask* current_task = snow_get_current_task();
	current_task->exception_handler = current_task->exception_handler->previous;
}
