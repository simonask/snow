#include "snow/continuation.h"
#include "snow/continuation-intern.h"
#include "snow/gc.h"
#include "snow/intern.h"

#include <string.h>

const size_t CONTINUATION_STACK_SIZE = 1 << 13; // 8K

static SnContinuation* _current_continuation = NULL;

static void set_current_continuation(SnContinuation* cc) {
	_current_continuation = cc;
}

void snow_init_current_continuation() {
	SnContinuation* cc = (SnContinuation*)snow_alloc_any_object(SN_CONTINUATION_TYPE, sizeof(SnContinuation));
	cc->function = NULL;
	cc->stack_hi = (byte*)(uintx)-1;
	cc->stack_lo = NULL;
	cc->running = true;
	cc->context = NULL;
	cc->return_val = NULL;
	set_current_continuation(cc);
}

SnContinuation* snow_get_current_continuation() {
	return _current_continuation;
}

SnContinuation* snow_create_continuation(SnFunctionPtr func, SnContext* context)
{
	SnContinuation* cc = (SnContinuation*)snow_alloc_any_object(SN_CONTINUATION_TYPE, sizeof(SnContinuation));
	cc->function = func;
	cc->stack_lo = (byte*)snow_gc_alloc(CONTINUATION_STACK_SIZE);
	cc->stack_hi = cc->stack_lo + CONTINUATION_STACK_SIZE;
	memset((void*)cc->stack_lo, 0xcd, CONTINUATION_STACK_SIZE);
	cc->running = false;
	cc->return_to = NULL;
	cc->context = context;
	cc->return_val = NULL;
	return cc;
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
	
	ASSERT(false);
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

void init_continuation_class(SnClass* klass)
{
	
}
