#include "snow/task.h"
#include "snow/task-intern.h"
#include "snow/continuation.h"
#include "snow/intern.h"
#include "snow/exception-intern.h"

#include <dispatch/dispatch.h>
#include <dispatch/group.h>
#include <dispatch/semaphore.h>
#include <pthread.h>

typedef struct SnDispatchThreadState {
	SnTask* current_task;
	dispatch_semaphore_t gc_barrier;
	struct SnDispatchThreadState* previous;
	struct SnDispatchThreadState* next;
} SnDispatchThreadState;

static pthread_key_t state_key;
static dispatch_semaphore_t state_lock = NULL;
static SnDispatchThreadState* state = NULL;

static SnDispatchThreadState* init_thread() {
	ASSERT(pthread_getspecific(state_key) == NULL);
	SnDispatchThreadState* s = (SnDispatchThreadState*)snow_malloc(sizeof(SnDispatchThreadState));
	s->current_task = NULL;
	s->gc_barrier = dispatch_semaphore_create(0); // starts locked
	s->previous = NULL;
	
	// prepend this state to the list of thread states
	dispatch_semaphore_wait(state_lock, DISPATCH_TIME_FOREVER);
	s->next = state;
	if (s->next) {
		ASSERT(s->next->previous == NULL);
		s->next->previous = s;
	}
	state = s;
	dispatch_semaphore_signal(state_lock);
	
	pthread_setspecific(state_key, s);
	return s;
}

static void finalize_thread(void* _state) {
	SnDispatchThreadState* s = (SnDispatchThreadState*)_state;
	dispatch_semaphore_signal(s->gc_barrier); // when finalizing a thread during gc, we don't want to wait for this.
	dispatch_semaphore_wait(state_lock, DISPATCH_TIME_FOREVER);
	if (s->next) s->next->previous = s->previous;
	if (s->previous) s->previous->next = s->next;
	state = s->next;
	dispatch_semaphore_signal(state_lock);
	dispatch_release(s->gc_barrier);
	snow_free(s);
}

static inline SnDispatchThreadState* get_state() {
	SnDispatchThreadState* s = (SnDispatchThreadState*)pthread_getspecific(state_key);
	if (!s) return init_thread();
	return s;
}

void snow_init_parallel(void* stack_top) {
	pthread_key_create(&state_key, finalize_thread);
	state_lock = dispatch_semaphore_create(1);
	
	static SnTask main_task;
	memset(&main_task, 0, sizeof(main_task));
	main_task.stack_top = stack_top;
	get_state()->current_task = &main_task;
}


SnTask* snow_get_current_task() {
	return get_state()->current_task;
}

uintx snow_get_current_task_id() {
	return (uintx)snow_get_current_task();
}

static inline void perform_task(SnTask* task, void(^func)()) {
	SnDispatchThreadState* s = get_state();
	SnTask* parent_task = s->current_task;
	
	if (parent_task) {
		snow_task_pause();
	}
	
	GET_STACK_PTR(task->stack_top);
	task->previous = parent_task;
	s->current_task = task;
	
	SnContinuation base;
	snow_continuation_init(&base, NULL, NULL);
	
	task->base = &base;
	task->base->task_id = (uintx)task;
	task->base->running = true;
	task->continuation = task->base;
	
	if (!snow_continuation_save_execution_state(task->base))
	{
		func();
	}
	
	s->current_task = task->previous;
	ASSERT(s->current_task == parent_task); // stack corruption?
	
	if (parent_task) {
		snow_task_resume();
	}
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
	SnTask _tasks[num_elements];
	memset(_tasks, 0, sizeof(_tasks));
	SnTask* tasks = _tasks;
	
	SnTask* current_task = get_state()->current_task;
	
	if (current_task->previous) {
		// already in parallel, don't spawn further threads
		for (size_t i = 0; i < num_elements; ++i) {
			perform_task(tasks + i, ^{
				func(data, element_size, i, userdata);
			});
		}
	} else {
		dispatch_queue_t q = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
		dispatch_apply(num_elements, q, ^(size_t i) {
			perform_task(tasks + i, ^{
				func(data, element_size, i, userdata);
			});
		});
	}
	
	collect_exceptions_and_rethrow(tasks, num_elements);
}

static inline void with_each_thread_do(void(^func)(SnDispatchThreadState*)) {
	for (SnDispatchThreadState* s = state; s != NULL; s = s->next) {
		func(s);
	}
}


// GC INTROSPECTION

void snow_task_pause() {
	SnTask* task = snow_get_current_task();
	ASSERT(task->stack_bottom == NULL); // pausing already hibernated task!
	GET_STACK_PTR(task->stack_bottom);
}

void snow_task_resume() {
	SnTask* task = snow_get_current_task();
	ASSERT(task->stack_bottom != NULL); // resuming non-hibernated task!
	task->stack_bottom = NULL;
}

void snow_gc_barrier_enter() {
	snow_task_pause();
	dispatch_semaphore_signal(get_state()->gc_barrier);
}

void snow_gc_barrier_leave() {
	dispatch_semaphore_wait(get_state()->gc_barrier, DISPATCH_TIME_FOREVER);
	snow_task_resume();
}

void snow_set_gc_barriers() {
	dispatch_semaphore_wait(state_lock, DISPATCH_TIME_FOREVER);
	SnDispatchThreadState* me = get_state();
	with_each_thread_do(^(SnDispatchThreadState* s) {
		if (s != me && s->current_task) {
			dispatch_semaphore_wait(s->gc_barrier, DISPATCH_TIME_FOREVER); // TODO: Emit a warning if this is taking too long.
		}
	});
}

void snow_unset_gc_barriers() {
	SnDispatchThreadState* me = get_state();
	with_each_thread_do(^(SnDispatchThreadState* s) {
		if (s != me && s->current_task) {
			dispatch_semaphore_signal(s->gc_barrier);
		}
	});
	dispatch_semaphore_signal(state_lock);
}

void snow_with_each_task_do(SnTaskIteratorFunc func, void* userdata) {
	with_each_thread_do(^(SnDispatchThreadState* s) {
		for (SnTask* task = s->current_task; task != NULL; task = task->previous) {
			func(task, userdata);
		}
	});
}

static void deferred_task_finalize(void* _task) {
	SnDeferredTask* task = (SnDeferredTask*)_task;
	dispatch_group_t group = (dispatch_group_t)task->private;
	dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
	dispatch_release(group);
}

SnDeferredTask* snow_deferred_call(VALUE closure) {
	__block SnDeferredTask* task = (SnDeferredTask*)snow_alloc_any_object(SN_DEFERRED_TASK_TYPE, sizeof(SnDeferredTask));
	snow_gc_set_free_func(task, deferred_task_finalize);
	task->closure = closure;
	task->result = NULL;
	dispatch_group_t group = dispatch_group_create();
	task->private = group;
	task->task = NULL;
	dispatch_queue_t q = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
	snow_gc_barrier_enter();
	dispatch_group_async(group, q, ^{
		SnTask t;
		memset(&t, 0, sizeof(t));
		task->task = &t;
		perform_task(task->task, ^{
			volatile SnDeferredTask* my_task = task; // root for gc
			my_task->result = snow_call(NULL, closure, 0);
		});
		task->task = NULL;
	});
	snow_gc_barrier_leave();
	return task;
}

VALUE snow_deferred_task_wait(SnDeferredTask* task) {
	// TODO: add custom timeout
	dispatch_group_wait((dispatch_group_t)task->private, DISPATCH_TIME_FOREVER);
	return task->result;
}

