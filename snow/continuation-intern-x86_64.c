#include "snow/continuation-intern.h"
#include "snow/arch-x86_64.h"

#if defined(__GNUC__)
#define NO_PROFILING __attribute__((no_instrument_function))
#else
#define NO_PROFILING
#endif

NOINLINE NO_PROFILING void _continuation_resume(SnContinuation* cc) {
	SnVolatileRegisters vreg;
	vreg.rdi = cc->context;
	snow_restore_execution_state_with_volatile_registers(&cc->internal->state, &vreg);
	TRAP(); // should never be reached
}

void NO_PROFILING _continuation_return_handler() {
	/*
		This is a bouncer function that catches when the outmost function in
		a continuation returns. It assumes that a pointer to the current continuation
		is stored at the top of the continuation's stack (stack_hi-0x8 or %rsp+0x10),
		which it then moves into the first argument register and calls snow_continuation_return,
		so instead of a regular leave/ret sequence, the return_to-continuation will be resumed.
	*/
	__asm__ __volatile__(
	"movq 0x10(%%rsp), %%rdi\n"
	"movq %%rax, %%rsi\n"
	"xorq %%rax, %%rax\n"
	"call _snow_continuation_return\n"
	: 
	: 
	: "%rax", "%rsi", "%rdi"
	);
}

void _continuation_reset(SnContinuation* cc) {
	(*(void**)(cc->internal->stack_hi-0x8)) = cc; // self-reference for return bouncer
	(*(void**)(cc->internal->stack_hi-0x10)) = NULL; // rbp for return bouncer
	CAST_FUNCTION_TO_DATA((*(void**)(cc->internal->stack_hi-0x18)), _continuation_return_handler); // bouncer
	cc->internal->state.rsp = cc->internal->stack_hi - 0x18; // alignment, bounce space
	cc->internal->state.rbp = cc->internal->state.rsp;
	cc->internal->running = false;
	cc->internal->state.rip = (void(*)())cc->function;
}
