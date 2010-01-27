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

SnException* snow_current_exception() {
	return snow_get_current_exception_handler()->exception;
}

static void snow_try_finish_resumptions(SnTryState* state) {
	ASSERT(state->started);
	
	snow_set_current_exception_handler(snow_get_current_exception_handler()->previous);
	state->started = false;
	
	if (state->exception_to_propagate)
		snow_throw_exception(state->exception_to_propagate);
}

SnExecutionState* snow_try_setup_resumptions(SnTryState* state) {
	state->started = true;
	state->resumption_state = SnTryResumptionStateTrying;
	state->exception_to_propagate = NULL;
	state->iteration_was_prepared = false;
	
	SnExceptionHandler* handler = snow_create_exception_handler();
	snow_set_current_exception_handler(handler);
	return &handler->state;
}

SnTryResumptionState snow_try_prepare_resumption_iteration(SnTryState* state) {
	ASSERT(state->started);
	
	switch (state->resumption_state) {
		case SnTryResumptionStateTrying:
			if (state->iteration_was_prepared) {
				// Try block threw; now, catch
				state->resumption_state = SnTryResumptionStateCatching;
			}
			break;
		case SnTryResumptionStateCatching:
			if (state->iteration_was_prepared) {
				// Catch block threw
				state->exception_to_propagate = snow_current_exception();
			}
			state->resumption_state = SnTryResumptionStateEnsuring;
			break;
		case SnTryResumptionStateEnsuring:
			if (state->iteration_was_prepared) {
				// Ensure block threw; just propagate
				state->exception_to_propagate = snow_current_exception();
				snow_try_finish_resumptions(state);
			}
			break;
	}
	
	state->iteration_was_prepared = true;
	return state->resumption_state;
}

void snow_end_try(SnTryState* state) {
	ASSERT(state->started);
	ASSERT(state->iteration_was_prepared);
	state->iteration_was_prepared = false;
	
	switch (state->resumption_state) {
		case SnTryResumptionStateTrying:
			// Came through the try block cleanly; time to ensure
			state->resumption_state = SnTryResumptionStateEnsuring;
			snow_restore_execution_state(&snow_get_current_exception_handler()->state);
			break;
		case SnTryResumptionStateCatching:
			// Caught cleanly. Ensure.
			state->resumption_state = SnTryResumptionStateEnsuring;
			snow_restore_execution_state(&snow_get_current_exception_handler()->state);
			break;
		case SnTryResumptionStateEnsuring:
			// Propagate exception if due, and we're done.
			snow_try_finish_resumptions(state);
			break;
	}
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

SNOW_FUNC(exception_current) {
	return snow_get_current_exception_handler()->exception;
}

SNOW_FUNC(exception_to_string) {
	ASSERT_TYPE(SELF, SN_EXCEPTION_TYPE);
	return ((SnException*)SELF)->description;
}

void init_exception_class(SnClass* klass)
{
	snow_define_class_property(klass, "current", exception_current, NULL);
	snow_define_method(klass, "to_string", exception_to_string);
}