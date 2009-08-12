#include "snow/continuation-intern.h"

#define NO_PROFILING __attribute__((flatten,no_instrument_function))

NOINLINE NO_PROFILING void _continuation_resume(SnContinuation* cc) {
	__asm__ __volatile__(
		// restore registers from saved continuation (cc)
		"movq %0, %%rax\n"	// regs in rax
		"movq %1, %%rdi\n"	// context in rdi
		"movq 0(%%rax), %%rbx\n"
		"movq 8(%%rax), %%rbp\n"
		"movq 16(%%rax), %%rsp\n"
		"movq 24(%%rax), %%r12\n"
		"movq 32(%%rax), %%r13\n"
		"movq 40(%%rax), %%r14\n"
		"movq 48(%%rax), %%r15\n"
		"movq 56(%%rax), %%r8\n"
		// indicate to function being resumed that it was resumed
		// the contents of %rax will be seen as the return value from _continuation save
		"movq $1, %%rax\n"
		// perform the jump
		"jmpq *%%r8\n"
	:
	:
		"r"(&cc->reg),
		"r"(cc->context)
	: "%rax", "%r8", "%rdi"
	);
	TRAP(); // should never be reached
}

NOINLINE NO_PROFILING bool _continuation_save(SnContinuation* cc)
{
	__asm__ __volatile__(
	"movq %0, %%rax\n" // regs in rax
	"movq %%rbx, 0(%%rax)\n"
	// rbp stored on the stack
	"movq (%%rbp), %%rcx\n"
	"movq %%rcx, 8(%%rax)\n"
	// rsp is rsp+0x10, since the stack size of this function is 0 + size of link space (saved %rbp and %rsp)
	"movq %%rsp, 16(%%rax)\n"
	"addq $0x10, 16(%%rax)\n"
	// other regs
	"movq %%r12, 24(%%rax)\n"
	"movq %%r13, 32(%%rax)\n"
	"movq %%r14, 40(%%rax)\n"
	"movq %%r15, 48(%%rax)\n"
	// read instruction pointer for calling function from the stack.
	// This will be the instruction pointer to the instruction after the `call` that entered _continuation_save.
	"movq %%rbp, %%r8\n"
	"movq 8(%%r8), %%rcx\n"
	"movq %%rcx, 56(%%rax)\n"
	// TODO: save all the non-volatile registers, not just rsp/rbp/rip.
	:
	: "r"(&cc->reg)
	: "%rax", "%rcx"
	);
	return false;	
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
	(*(void**)(cc->stack_hi-0x8)) = cc; // self-reference for return bouncer
	(*(void**)(cc->stack_hi-0x10)) = NULL; // rbp for return bouncer
	(*(void**)(cc->stack_hi-0x18)) = _continuation_return_handler; // bouncer
	cc->reg.rsp = cc->stack_hi - 0x18; // alignment, bounce space
	cc->reg.rbp = cc->reg.rsp;
	cc->running = false;
	cc->reg.rip = cc->function;
}
