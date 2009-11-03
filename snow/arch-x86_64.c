#include "snow/arch-x86_64.h"

static SnVolatileRegisters null_registers = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};

bool snow_save_execution_state(SnExecutionState* state)
{
	__asm__ __volatile__(
		"movq %0, %%rax\n" // state ptr
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
		// This will be the instruction pointer to the instruction after the `call` that entered snow_save_execution_state.
		"movq %%rbp, %%r8\n"
		"movq 8(%%r8), %%rcx\n"
		"movq %%rcx, 56(%%rax)\n"
	:: "r"(state)
	: "%rax", "%rcx"
	);
	return false;
}

void snow_restore_execution_state(SnExecutionState* state)
{
	__asm__ __volatile__(
		"movq %0, %%rax\n"    // state ptr
		// restore non-volatile regs (mostly for providing arguments)
		"movq 0(%%rax), %%rbx\n"
		"movq 8(%%rax), %%rbp\n"
		"movq 16(%%rax), %%rsp\n"
		"movq 24(%%rax), %%r12\n"
		"movq 32(%%rax), %%r13\n"
		"movq 40(%%rax), %%r14\n"
		"movq 48(%%rax), %%r15\n"
		"movq 56(%%rax), %%r11\n" // rip
		// return value for snow_save_execution_state
		"movq $1, %%rax\n"
		// perform the jump
		"jmpq *%%r11"
	:: "r"(state)
	: "%rax", "%r8", "%rdi"
	);
}

void snow_restore_execution_state_with_volatile_registers(SnExecutionState* state, SnVolatileRegisters* vreg)
{
	if (!vreg) vreg = &null_registers;
	
	__asm__ __volatile__(
		"movq %0, %%rax\n"    // state ptr
		"movq %1, %%r11\n"    // volatile regs ptr
		// restore volatile regs
		"movq 0(%%r11), %%rcx\n"
		"movq 8(%%r11), %%rdx\n"
		"movq 16(%%r11), %%rsi\n"
		"movq 24(%%r11), %%rdi\n"
		"movq 32(%%r11), %%r8\n"
		"movq 40(%%r11), %%r9\n"
		"movq 48(%%r11), %%r10\n"
		// restore non-volatile regs (mostly for providing arguments)
		"movq 0(%%rax), %%rbx\n"
		"movq 8(%%rax), %%rbp\n"
		"movq 16(%%rax), %%rsp\n"
		"movq 24(%%rax), %%r12\n"
		"movq 32(%%rax), %%r13\n"
		"movq 40(%%rax), %%r14\n"
		"movq 48(%%rax), %%r15\n"
		"movq 56(%%rax), %%r11\n" // rip
		// return value for snow_save_execution_state
		"movq $1, %%rax\n"
		// perform the jump
		"jmpq *%%r11"
	:: "r"(state), "r"(vreg)
	: "%rax", "%r8", "%rdi"
	);
	
}