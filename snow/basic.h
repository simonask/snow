#ifndef BASIC_H_N5K8EFY5
#define BASIC_H_N5K8EFY5

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>


#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define MAKE_VERSION(major, minor, patch) (major * 1000000 + minor * 1000 + patch)
#define GCC_VERSION MAKE_VERSION(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)

#ifdef __cplusplus__
#define CAPI extern "C"
#else
#define CAPI 
#endif

#define NOINLINE __attribute__((noinline))
#define HIDDEN __attribute__((visibility ("hidden")))
#define USED __attribute__((used))
#define PACKED __attribute__((packed))
#define ALIGN(N) __attribute__((aligned(N)))
#define PURE __attribute__((pure))

#if GCC_VERSION >= MAKE_VERSION(4, 4, 0)
#define ATTR_HOT __attribute__((hot))
#else
#define ATTR_HOT
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
typedef int32_t inth;
typedef uint32_t uinth;
#define INTX_MIN LLONG_MIN
#define INTX_MAX LLONG_MAX
#define UINTX_MAX ULLONG_MAX
#define INTH_MIN INT_MIN
#define INTH_MAX INT_MAX
#define UINTH_MAX UINT_MAX
#else
typedef int32_t intx;
typedef uint32_t uintx;
typedef int16_t inth;
typedef uint16_t uinth;
#define INTX_MIN INT_MIN
#define INTX_MAX INT_MAX
#define UINTX_MAX UINT_MAX
#define INTH_MIN SHRT_MIN
#define INTH_MAX SHRT_MAX
#define UINTH_MAX USHRT_MAX
#endif

#ifndef byte
typedef unsigned char byte;
#endif
typedef void* VALUE ALIGN(sizeof(void*));

struct array_t {
	// this array type is supposed to be used by more advanced structures wishing to 
	// implement array-like data structures without allocating an SnArray, which would
	// pointer indirection. See array-intern.h for functions manipulating this struct.
	
	VALUE* data; // must be first element!
	uint32_t size;
	uint32_t alloc_size;
};

// the following hullaballoo is necessary because ISO C99 doesn't allow casts between data and function pointers. Snow does that a lot.
union function_pointer_caster_t {
	void* data;
	void(*function)();
};
typedef void(*basic_function_t)();
#define CAST_FUNCTION_TO_DATA(DATA, FUNCTION) union function_pointer_caster_t _caster ##__LINE__; _caster##__LINE__.function = (void(*)())(FUNCTION); *((void**)&(DATA)) = _caster##__LINE__.data
#define CAST_DATA_TO_FUNCTION(FUNCTION, DATA) union function_pointer_caster_t _caster ##__LINE__; _caster##__LINE__.data = (void*)(DATA); *((basic_function_t*)&(FUNCTION)) = _caster##__LINE__.function

#endif /* end of include guard: BASIC_H_N5K8EFY5 */
