#include "snow/continuation-intern.h"

NOINLINE void _continuation_resume(SnContinuation* cc) {
	__asm__ __volatile__(
		// restore registers
		"movq %0, %%rdi\n"
		"movq %1, %%rsp\n"
		"movq %2, %%rbp\n"
		// indicate to function being resumed that it was resumed
		"movq $1, %%rax\n"
		// resume!
		"movq %3, %%r8\n"
		"jmpq *%%r8\n"
	:
	: "r"(cc), "m"(cc->reg.rsp), "m"(cc->reg.rbp), "m"(cc->reg.rip)
	: "%rax", "%rdi", "%rsi", "%rsp", "%r8"
	);
}

NOINLINE bool _continuation_save(SnContinuation* cc) {
	__asm__ __volatile__(
	// rsp is rbp+0x10, since the stack size of this function is 0
	"movq %%rsp, %0\n"
	"addq $0x10, %0\n"
	// rbp stored on the stack
	"movq (%%rbp), %%rax\n"
	"movq %%rax, %1\n"
	 // read instruction pointer for calling function from the stack
	"movq %%rbp, %%r8\n"
	"movq 8(%%r8), %%rax\n"
	"movq %%rax, %2\n"
	// TODO: save other registers?
	: "=m"(cc->reg.rsp), "=m"(cc->reg.rbp), "=m"(cc->reg.rip)
	:
	: "%rax"
	);
	return false;	
}

NOINLINE static void _continuation_return_handler() {
	// this function is meant to be jumped to by a `ret`-instruction.
	// Continuations don't like the C way of call/return, so this
	// cheats C to return the continuation way anyway.
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
	(*(void**)(cc->stack_hi-0x8)) = cc; // hidden self-reference for _continuation_return_handler
	(*(void**)(cc->stack_hi-0x10)) = NULL; // rbp for _continuation_return_handler
	(*(void**)(cc->stack_hi-0x18)) = _continuation_return_handler;
	cc->reg.rsp = cc->stack_hi - 0x18; // alignment
	cc->reg.rbp = cc->reg.rsp;
	cc->running = false;
	cc->reg.rip = cc->function;
}
