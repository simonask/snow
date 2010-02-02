#ifndef ARCH_H_QW6ZZE35
#define ARCH_H_QW6ZZE35

#if defined(ARCH_x86_64)
#include "snow/arch-x86_64.h"
#else
#error Unsupported architecture. Supported build architecures: x86-64
#endif


#define ASSERT(x) do { if (!(x)) TRAP(); } while (0)
#define ASSERT_TYPE(OBJECT, TYPE) ASSERT(snow_typeof(OBJECT) == (TYPE))

#endif /* end of include guard: ARCH_H_QW6ZZE35 */
