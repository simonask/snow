#ifndef BASIC_H_N5K8EFY5
#define BASIC_H_N5K8EFY5

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus__
#define CAPI extern "C"
#else
#define CAPI 
#endif

#define NOINLINE __attribute__((noinline))
#define ATTR_HOT __attribute__((hot))
#define HIDDEN __attribute__((visibility ("hidden")))
#define USED __attribute__((used))


#ifdef ARCH_x86_64
#define ARCH_IS_64_BIT 1
#endif

#ifdef ARCH_x86_32
#define ARCH_IS_32_BIT 1
#endif

#ifdef ARCH_IS_64_BIT
typedef int64_t intx;
typedef uint64_t uintx;
typedef int32_t inth;
typedef uint32_t uinth;
#else
typedef int32_t intx;
typedef uint32_t uintx;
typedef int16_t inth;
typedef uint16_t uinth;
#endif

#ifndef byte
typedef unsigned char byte;
#endif
typedef void* VALUE;

struct array_t {
	// this array type is supposed to be used by more advanced structures wishing to 
	// implement array-like data structures without allocating an SnArray, which would
	// pointer indirection. See array-intern.h for functions manipulating this struct.
	VALUE* data;
	uinth size;
	uinth alloc_size;
};

#endif /* end of include guard: BASIC_H_N5K8EFY5 */
