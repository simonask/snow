#ifndef CONTINUATION_H_C8DBOWFC
#define CONTINUATION_H_C8DBOWFC

#include "snow/object.h"
#include "snow/arch.h"
#include "snow/context.h"
#include "snow/task.h"

typedef struct SnContinuation {
	SnObjectBase base;
	SnFunctionPtr function;
	SnRegisters reg;	// saved registers
	byte* stack_hi;
	byte* stack_lo;
	bool running;
	bool interruptible;
	struct SnContinuation* return_to;
	struct SnContinuation* please_clean;

	SnContext* context;
	VALUE return_val;
} SnContinuation;

CAPI void snow_init_main_continuation();

CAPI SnContinuation* snow_create_continuation(SnFunctionPtr, SnContext* context);
CAPI void snow_continuation_init(SnContinuation*, SnFunctionPtr, SnContext*)         ATTR_HOT;
CAPI VALUE snow_continuation_call(SnContinuation*, SnContinuation* return_to)        ATTR_HOT;
CAPI void snow_continuation_yield(SnContinuation* cc, VALUE val)                     ATTR_HOT;
CAPI void snow_continuation_return(SnContinuation* cc, VALUE val)                    ATTR_HOT;
CAPI void snow_continuation_resume(SnContinuation* cc)                               ATTR_HOT;
CAPI void snow_continuation_get_stack_bounds(SnContinuation* cc, byte** lo, byte** hi);

static inline void snow_continuation_stack_guard() {
	byte* sp;
	GET_STACK_PTR(sp);
	SnContinuation* cc = snow_get_current_continuation();
	if (cc && (sp < cc->stack_lo)) {
		TRAP(); // Heap corruption! Check for infinite recursion. Otherwise, try increasing the desired stack size of your continuation.
	}
}

static inline intx snow_current_continuation_available_stack_space()
{
	SnContinuation* cc = snow_get_current_continuation();
	intx lo = (intx)cc->stack_lo;
	intx current = 0;
	GET_STACK_PTR(current);
	return current - lo;
}

#endif /* end of include guard: CONTINUATION_H_C8DBOWFC */
