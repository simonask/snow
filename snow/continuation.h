#ifndef CONTINUATION_H_C8DBOWFC
#define CONTINUATION_H_C8DBOWFC

#include "snow/object.h"
#include "snow/arch.h"
#include "snow/context.h"

typedef struct SnContinuation {
	SnObjectBase base;
	
	SnFunctionPtr function;
	
	#ifdef ARCH_x86_64
	struct {
		void* rbx;
		void* rbp;
		void* rsp;
		void* r12;
		void* r13;
		void* r14;
		void* r15;
		void* rip;
	} reg;
	#endif
	
	byte* stack_hi;
	byte* stack_lo;
	bool running;
	struct SnContinuation* return_to;

	SnContext* context;
	VALUE return_val;
} SnContinuation;

CAPI void snow_init_current_continuation();
CAPI SnContinuation* snow_get_current_continuation();

CAPI SnContinuation* snow_create_continuation(SnFunctionPtr, SnContext* context);
CAPI VALUE snow_continuation_call(SnContinuation*, SnContinuation* return_to)        ATTR_HOT;
CAPI void snow_continuation_yield(SnContinuation* cc, VALUE val)                     ATTR_HOT;
CAPI void snow_continuation_return(SnContinuation* cc, VALUE val)                    ATTR_HOT;
CAPI void snow_continuation_resume(SnContinuation* cc)                               ATTR_HOT;

#endif /* end of include guard: CONTINUATION_H_C8DBOWFC */
