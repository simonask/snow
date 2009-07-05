#ifndef BASIC_H_N5K8EFY5
#define BASIC_H_N5K8EFY5

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus__
#define CAPI extern "C"
#else
#define CAPI 
#endif


#ifdef ARCH_x86_64
#define ARCH_IS_64_BIT 1
#endif

#ifdef ARCH_x86_32
#define ARCH_IS_32_BIT 1
#endif

#ifdef ARCH_IS_64_BIT
typedef int64_t intx;
typedef uint64_t uintx;
#else
typedef int32_t intx;
typedef uint32_t uintx;
#endif

typedef unsigned char byte;

#endif /* end of include guard: BASIC_H_N5K8EFY5 */
