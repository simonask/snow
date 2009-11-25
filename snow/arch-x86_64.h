#ifndef ARCH_X86_64_H_1DPMYG2I
#define ARCH_X86_64_H_1DPMYG2I

#include "snow/basic.h"

#define GET_STACK_PTR(var) __asm__("mov %%rsp, %0\n" : "=g"(var))
#define GET_BASE_PTR(var) __asm__("mov %%rbp, %0\n" : "=g"(var))
#define GET_RETURN_PTR(var) __asm__("mov 8(%%rbp), %0\n" : "=r"(var))
#define TRAP() __asm__("int3\nnop\n")

typedef struct SnExecutionState {
	void* rbx;
	void* rbp;
	void* rsp;
	void* r12;
	void* r13;
	void* r14;
	void* r15;
	void(*rip)();
} SnExecutionState;

typedef struct SnVolatileRegisters {
	// rax is return val for snow_save_execution_state, and scratch reg, so cannot be restored.
	// r11 is used as temporary storage for rip, so cannot be restored.
	void* rcx; // 4th arg
	void* rdx; // 3rd arg
	void* rsi; // 2nd arg
	void* rdi; // 1st arg
	void* r8;  // 5th arg
	void* r9;  // 6th arg
	void* r10;
} SnVolatileRegisters;

CAPI bool snow_save_execution_state(SnExecutionState* state); // false = saved, true = returned from restoration
CAPI void snow_restore_execution_state(SnExecutionState* state);
CAPI void snow_restore_execution_state_with_volatile_registers(SnExecutionState* state, SnVolatileRegisters* volatile_regs);
static inline void* snow_execution_state_get_stack_pointer(SnExecutionState* state) { return state->rsp; }

static inline bool snow_bit_test(void* field, uintx bit_index) {
	bool already_set = false;
	__asm__ __volatile__(
		"bt %2, %1\n"
		"adc $0, %0\n"
		: "=g"(already_set), "=m"(*(byte*)field)
		: "Ir"(bit_index)
	);
	return already_set;
}

static inline bool snow_bit_test_and_set(void* field, uintx bit_index) {
	bool already_set = false;
	__asm__ __volatile__(
		"lock bts %2, %1\n"
		"adc $0, %0\n"
		: "=g"(already_set), "=m"(*(byte*)field)
		: "Ir"(bit_index)
	);
	return already_set;
}

static inline bool snow_bit_test_and_clear(void* field, uintx bit_index) {
	bool already_set = false;
	__asm__ __volatile__(
		"lock btr %2, %1\n"
		"adc $0, %0\n"
		: "=g"(already_set), "=m"(*(byte*)field)
		: "Ir"(bit_index)
	);
	return already_set;
}

#define SAFE_INCR_BYTE(x) __asm__("lock incb %0\n" : "=m"(x))
#define SAFE_DECR_BYTE(x) __asm__("lock decb %0\n" : "=m"(x))
#define SAFE_INCR_WORD(x) __asm__("lock incq %0\n" : "=m"(x))
#define SAFE_DECR_WORD(x) __asm__("lock decq %0\n" : "=m"(x))



#endif /* end of include guard: ARCH_X86_64_H_1DPMYG2I */
