#include "snow/task.h"
#include "snow/task-intern.h"
#include "snow/intern.h"
#include "snow/exception-intern.h"
#include "snow/continuation.h"
#include "snow/class.h"

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

struct SnContinuation* snow_get_current_continuation() {
	return snow_get_current_task()->continuation;
}

void snow_set_current_continuation(SnContinuation* cc)
{
	SnTask* current_task = snow_get_current_task();
//	ASSERT((uintx)current_task == cc->task_id);
	current_task->continuation = cc;
}

void snow_abort_current_task(VALUE exception)
{
	SnTask* current_task = snow_get_current_task();
	ASSERT(current_task);
	ASSERT(current_task->exception == NULL); // inconsistency: task should already be aborted at this point
	current_task->exception = exception;
	snow_continuation_resume(current_task->base);
}

/*
	SNOW API
*/

SNOW_FUNC(deferred_task_new) {
	REQUIRE_ARGS(1);
	return snow_deferred_call(ARGS[0]);
}

SNOW_FUNC(deferred_task_wait) {
	ASSERT_TYPE(SELF, SnDeferredTaskType);
	return snow_deferred_task_wait((SnDeferredTask*)SELF);
}

void SnDeferredTask_init_class(SnClass* klass) {
	snow_define_class_method(klass, "__call__", deferred_task_new);
	snow_define_method(klass, "wait", deferred_task_wait);
	snow_define_property(klass, "result", deferred_task_wait, NULL);
}
