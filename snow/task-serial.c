#include "snow.h"
#include "snow/task.h"
#include "snow/task-intern.h"
#include "snow/array.h"
#include "snow/exception-intern.h"
#include "snow/continuation.h"


static SnTask* current_task = NULL;
static bool gc_barrier = false;

void snow_init_parallel(void* stack_top) {
	static SnTask main_task;
	memset(&main_task, 0, sizeof(main_task));
	main_task.stack_top = stack_top;
	current_task = &main_task;
}

void snow_gc_barrier_enter() {
	ASSERT(!gc_barrier);
	gc_barrier = true;
}

void snow_gc_barrier_leave() {
	ASSERT(gc_barrier);
	gc_barrier = false;
}

SnTask* snow_get_current_task() {
	return current_task;
}

void snow_with_each_task_do(SnTaskIteratorFunc func, void* userdata) {
	for (SnTask* t = current_task; t != NULL; t = t->previous) {
		func(t, userdata);
	}
}

void snow_set_gc_barriers() {
	gc_barrier = true;
}

void snow_unset_gc_barriers() {
	gc_barrier = false;
}

uintx snow_get_current_task_id() {
	return (uintx)current_task;
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
	
	for (size_t i = 0; i < num_elements; ++i) {
		SnTask* task = tasks + i;
		
		GET_STACK_PTR(task->stack_top);
		task->previous = current_task;
		current_task = task;

		SnContinuation base;
		snow_continuation_init(&base, NULL, NULL);

		task->base = &base;
		task->base->task_id = (uintx)task;
		task->base->running = true;
		task->continuation = task->base;

		if (!snow_continuation_save_execution_state(task->base))
		{
			func(data, element_size, i, userdata);
		}

		current_task = task->previous;
	}
	
	collect_exceptions_and_rethrow(tasks, num_elements);
}

SnDeferredTask* snow_deferred_call(VALUE closure) {
	SnDeferredTask* task = (SnDeferredTask*)snow_alloc_any_object(SN_DEFERRED_TASK_TYPE, sizeof(SnDeferredTask));
	task->closure = closure;
	task->result = NULL;
	task->_private = NULL;
	task->task = NULL;
	return task;
}

VALUE snow_deferred_task_wait(SnDeferredTask* task) {
	// TODO: add custom timeout
	if (task->result == NULL) {
		task->result = snow_call(NULL, task->closure, 0);
	}
	return task->result;
}
