#include "snow/continuation.h"
#include "snow/continuation-intern.h"
#include "snow/gc.h"
#include "snow/intern.h"
#include "snow/fixed-alloc.h"
#include "snow/task.h"
#include "snow/task-intern.h"

#include <string.h>

static void continuation_gc_cleanup(VALUE vcc);
static void continuation_init_stack(SnContinuation*);

#define CONTINUATION_STACK_SIZE  (1 << 16) // 64K
static FIXED_ALLOCATOR(stack_alloc, CONTINUATION_STACK_SIZE);

SnContinuation* snow_create_continuation(SnFunctionPtr func, SnContext* context)
{
	SnContinuation* cc = (SnContinuation*)snow_alloc_any_object(SN_CONTINUATION_TYPE, sizeof(SnContinuation));
	snow_continuation_init(cc, func, context);
	snow_gc_set_free_func(cc, continuation_gc_cleanup);
	return cc;
}

void snow_continuation_init(SnContinuation* cc, SnFunctionPtr func, SnContext* context)
{
	cc->function = func;
	cc->stack_lo = NULL;
	cc->stack_hi = NULL;
	cc->running = false;
	cc->interruptible = true;
	cc->return_to = NULL;
	cc->please_clean = NULL;
	cc->context = context;
	cc->return_val = NULL;
	cc->task_id = snow_get_current_task_id();
}

void snow_continuation_cleanup(SnContinuation* cc)
{
	ASSERT(!cc->running);
	
	fixed_free(&stack_alloc, cc->stack_lo);
	cc->stack_lo = NULL;
	cc->stack_hi = NULL;
}

static void continuation_init_stack(SnContinuation* cc)
{
	ASSERT(!cc->stack_lo);
	cc->stack_lo = (byte*)fixed_alloc(&stack_alloc);
	cc->stack_hi = cc->stack_lo + CONTINUATION_STACK_SIZE;
}

static void continuation_gc_cleanup(VALUE vcc)
{
	SnContinuation* cc = (SnContinuation*)vcc;
	ASSERT_TYPE(cc, SN_CONTINUATION_TYPE);
	snow_continuation_cleanup(cc);
}

VALUE snow_continuation_call(SnContinuation* cc, SnContinuation* return_to)
{
	ASSERT(cc != return_to);
	ASSERT(!cc->please_clean);
	
	cc->return_to = return_to;
	if (!snow_continuation_save_execution_state(return_to)) {
		// resuming
		snow_continuation_resume(cc);
	} else {
		// came back
		if (return_to->please_clean)
		{
			snow_continuation_cleanup(return_to->please_clean);
			return_to->please_clean = NULL;
		}
		return cc->return_val;
	}
	
	ASSERT(false);
	return NULL; // suppress warning
}

void snow_continuation_yield(SnContinuation* cc, VALUE val)
{
	ASSERT(cc->interruptible);
	if (snow_continuation_save_execution_state(cc)) {
		TRAP();
		// was restarted after yield
		return;
	}
	cc->return_val = val;
	ASSERT(cc->return_to->running);
	ASSERT(!cc->return_to->please_clean);
	snow_continuation_resume(cc->return_to);
}

void snow_continuation_return(SnContinuation* cc, VALUE val)
{
	ASSERT(cc->interruptible);
	cc->running = false;
	cc->return_val = val;
	
	// continuations cannot clean up themselves without messing up their
	// stacks, so they need the calling continuation to do it for them.
	ASSERT(!cc->return_to->please_clean);
	cc->return_to->please_clean = cc;
	
	ASSERT(cc->return_to->running);
	snow_continuation_resume(cc->return_to);
}

void snow_continuation_resume(SnContinuation* cc)
{
	SnContinuation* current = snow_get_current_continuation();
	ASSERT(cc->task_id == current->task_id); // can't resume continuations across threads/tasks
	snow_set_current_continuation(cc);
	
	if (!cc->running)
	{
		snow_continuation_cleanup(cc);
		continuation_init_stack(cc);
		_continuation_reset(cc);
		cc->running = true;
	}
	
	if (snow_task_is_base_continuation(cc)) {
//		snow_thread_returning_to_system_stack();
	} else {
//		snow_thread_departing_from_system_stack();
	}
	
	_continuation_resume(cc);
}

void snow_continuation_get_stack_bounds(SnContinuation* cc, byte** lo, byte** hi)
{
	*hi = cc->stack_hi;
	// TODO: Specialise per arch
	*lo = cc->state.rsp;
}

void init_continuation_class(SnClass* klass)
{
	
}
