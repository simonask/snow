#ifndef ARCH_H_QW6ZZE35
#define ARCH_H_QW6ZZE35

#ifdef ARCH_x86_64
#define GET_STACK_PTR(var) __asm__("mov %%rsp, %0" : "=r"(var))
#define GET_BASE_PTR(var) __asm__("mov %%rbp, %0" : "=r"(var))
#define TRAP() __asm__("int3\nnop\n")

typedef struct SnRegisters {
	void* rbx;
	void* rbp;
	void* rsp;
	void* r12;
	void* r13;
	void* r14;
	void* r15;
	void* rip;
} SnRegisters;
#endif

#endif /* end of include guard: ARCH_H_QW6ZZE35 */
