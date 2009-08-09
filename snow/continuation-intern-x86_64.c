#include "snow/continuation-intern.h"

NOINLINE void _continuation_resume(SnContinuation* cc) {
	__asm__ __volatile__(
		// restore registers from saved continuation (cc)
		// TODO: restore all the non-volatile registers
		"movq %0, %%rdi\n"
		"movq %1, %%rsp\n"
		"movq %2, %%rbp\n"
		// indicate to function being resumed that it was resumed
		// the contents of %rax will be seen as the return value from _continuation save
		"movq $1, %%rax\n"
		// perform the jump
		"movq %3, %%r8\n"
		"jmpq *%%r8\n"
	:
	: "m"(cc->context), "m"(cc->reg.rsp), "m"(cc->reg.rbp), "m"(cc->reg.rip)
	: "%rax", "%rdi", "%rsi", "%rsp", "%r8"
	);
	TRAP(); // should never be reached
}

NOINLINE bool _continuation_save(SnContinuation* cc)
{
	__asm__ __volatile__(
	// rsp is rbp+0x10, since the stack size of this function is 0 + size of link space (saved %rbp and %rsp)
	"movq %%rsp, %0\n"
	"addq $0x10, %0\n"
	// rbp stored on the stack
	"movq (%%rbp), %%rax\n"
	"movq %%rax, %1\n"
	// read instruction pointer for calling function from the stack.
	// This will be the instruction pointer to the instruction after the `call` that entered _continuation_save.
	"movq %%rbp, %%r8\n"
	"movq 8(%%r8), %%rax\n"
	"movq %%rax, %2\n"
	// TODO: save all the non-volatile registers, not just rsp/rbp/rip.
	: "=m"(cc->reg.rsp), "=m"(cc->reg.rbp), "=m"(cc->reg.rip)
	:
	: "%rax"
	);
	return false;	
}

NOINLINE __attribute__((flatten,no_instrument_function)) void _continuation_return_handler();

void _continuation_return_handler() {
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
	(*(void**)(cc->stack_hi-0x8)) = cc; // self-reference for return bouncer
	(*(void**)(cc->stack_hi-0x10)) = NULL; // rbp for return bouncer
	(*(void**)(cc->stack_hi-0x18)) = _continuation_return_handler; // bouncer
	cc->reg.rsp = cc->stack_hi - 0x18; // alignment, bounce space
	cc->reg.rbp = cc->reg.rsp;
	cc->running = false;
	cc->reg.rip = cc->function;
}
