#ifndef CONTINUATION_H_C8DBOWFC
#define CONTINUATION_H_C8DBOWFC

#include "snow/object.h"
#include "snow/arch.h"

struct SnContinuation;
typedef VALUE(*SnContinuationFunc)(struct SnContinuation*);

typedef struct SnContinuation {
	SnObjectBase base;
	
	SnContinuationFunc function;
	
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

	VALUE self;
	VALUE* args;
	uintx num_args;
	VALUE return_val;
} SnContinuation;

CAPI void snow_init_current_continuation();
CAPI SnContinuation* snow_get_current_continuation();

CAPI SnContinuation* snow_create_continuation(SnContinuationFunc);
CAPI void snow_continuation_set_self(SnContinuation*, VALUE self);
CAPI void snow_continuation_set_arguments(SnContinuation*, uintx num, VALUE* args);
CAPI VALUE snow_continuation_call(SnContinuation*, SnContinuation* return_to);
CAPI void snow_continuation_yield(SnContinuation* cc, VALUE val);
CAPI void snow_continuation_return(SnContinuation* cc, VALUE val);
CAPI void snow_continuation_resume(SnContinuation* cc);

#endif /* end of include guard: CONTINUATION_H_C8DBOWFC */
