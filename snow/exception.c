#include "snow/intern.h"
#include "snow/exception.h"
#include "snow/exception-intern.h"
#include "snow/class.h"
#include "snow/task-intern.h"
#include "snow/arch.h"
#include "snow/str.h"
#include "snow/snow.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void snow_throw_exception(VALUE exception) 
{
	SnExceptionHandler* handler = snow_get_current_task()->exception_handler;
	if (handler != NULL) {
		handler->exception = exception;
		snow_restore_execution_state(&handler->state);
	} else {
		snow_abort_current_task(exception);
	}
}

void snow_throw_exception_with_description(const char* description, ...)
{
	va_list ap;
	va_start(ap, description);
	char* str = NULL;
	vasprintf(&str, description, ap);
	va_end(ap);
	SnString* sstr = snow_create_string(str);
	free(str);
	SnException* ex = snow_create_exception();
	ex->description = sstr;
	snow_throw_exception(ex);
}

void snow_try_catch_ensure(SnExceptionTryFunc try_func, SnExceptionCatchFunc catch_func, SnExceptionEnsureFunc ensure_func, void* userdata)
{
	SnExceptionHandler handler;
	handler.exception = NULL;
	SnTask* current_task = snow_get_current_task();
	handler.previous = current_task->exception_handler;
	current_task->exception_handler = &handler;
	if (!snow_save_execution_state(&handler.state)) {
		try_func(userdata);
	} else {
		// Argh, exception thrown! Run the catch_func in a special exception handler (so we can do ensure)
		SnExceptionHandler catcher;
		catcher.exception = NULL;
		catcher.previous = &handler;
		current_task->exception_handler = &catcher;
		if (!snow_save_execution_state(&catcher.state)) {
			catch_func(handler.exception, userdata);
		} else {
			// catch block threw an exception, so run the ensure_func and propagate
			current_task->exception_handler = handler.previous;
			if (ensure_func) ensure_func(userdata);
			snow_throw_exception(catcher.exception);
		}
		// catch block didn't throw an exception, so pop exception handler and run ensure_func
		current_task->exception_handler = handler.previous;
		if (ensure_func) ensure_func(userdata); // XXX: What about rethrow?
		return;
	}
	// No exception thrown
	current_task->exception_handler = handler.previous;
	if (ensure_func) ensure_func(userdata);
}

SnExceptionHandler* snow_create_exception_handler()
{
	SnExceptionHandler* handler = snow_gc_alloc(sizeof(SnExceptionHandler));
	handler->previous = snow_get_current_exception_handler();
	handler->exception = NULL;
	return handler;
}

SnException* snow_create_exception()
{
	SnException* ex = (SnException*)snow_alloc_any_object(SN_EXCEPTION_TYPE, sizeof(SnException));
	snow_object_init((SnObject*)ex, snow_get_prototype(SN_EXCEPTION_TYPE));
	ex->description = NULL;
	ex->thrown_by = snow_get_current_continuation();
	return ex;
}

SNOW_FUNC(exception_to_string)
{
	ASSERT_TYPE(SELF, SN_EXCEPTION_TYPE);
	return ((SnException*)SELF)->description;
}

void init_exception_class(SnClass* klass)
{
	snow_define_method(klass, "to_string", exception_to_string);
}