#include "snow/continuation.h"
#include "snow/continuation-intern.h"
#include "snow/gc.h"
#include "snow/intern.h"

#include <string.h>

const size_t CONTINUATION_STACK_SIZE = 8192;

static SnContinuation* _current_continuation = NULL;

static void set_current_continuation(SnContinuation* cc) {
	_current_continuation = cc;
}

void snow_init_current_continuation() {
	SnContinuation* cc = snow_create_continuation(NULL);
	cc->running = true;
	set_current_continuation(cc);
	debug("current continuation is: 0x%llx\n", cc);
}

SnContinuation* snow_get_current_continuation() {
	return _current_continuation;
}

SnContinuation* snow_create_continuation(SnContinuationFunc func)
{
	SnContinuation* cc = (SnContinuation*)snow_alloc_any_object(SN_CONTINUATION_TYPE, sizeof(SnContinuation));
	cc->function = func;
	cc->stack_lo = (byte*)snow_gc_alloc(CONTINUATION_STACK_SIZE);
	cc->stack_hi = cc->stack_lo + CONTINUATION_STACK_SIZE;
	memset(cc->stack_lo, 0xcd, CONTINUATION_STACK_SIZE);
	cc->running = false;
	cc->return_to = NULL;
	cc->self = NULL;
	cc->args = NULL;
	cc->num_args = 0;
	cc->return_val = NULL;
}

void snow_continuation_set_self(SnContinuation* cc, VALUE self)
{
	cc->self = self;
}

void snow_continuation_set_arguments(SnContinuation* cc, uintx num, VALUE* args)
{
	// XXX: Copy args? Put in array?
	cc->num_args = num;
	cc->args = args;
}

VALUE snow_continuation_call(SnContinuation* cc, SnContinuation* return_to)
{
	ASSERT(cc != return_to);
	
	cc->return_to = return_to;
	if (!_continuation_save(return_to)) {
		// resuming
		snow_continuation_resume(cc);
	} else {
		// came back
		return cc->return_val;
	}
	
	return NULL; // suppress warning
}

void snow_continuation_yield(SnContinuation* cc, VALUE val)
{
	if (_continuation_save(cc)) {
		// was restarted after yield
		return;
	}
	cc->return_val = val;
	ASSERT(cc->return_to->running);
	snow_continuation_resume(cc->return_to);
}

void snow_continuation_return(SnContinuation* cc, VALUE val)
{
	cc->running = false;
	cc->return_val = val;
	ASSERT(cc->return_to->running);
	snow_continuation_resume(cc->return_to);
}

void snow_continuation_resume(SnContinuation* cc)
{
	set_current_continuation(cc);
	
	if (!cc->running)
		_continuation_reset(cc);
	cc->running = true;
	_continuation_resume(cc);
}
