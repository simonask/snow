#include "snow/task.h"
#include "snow/task-intern.h"
#include "snow/exception-intern.h"
#include "snow/continuation.h"

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
	ASSERT((uintx)current_task == cc->task_id);
	current_task->continuation = cc;
}
