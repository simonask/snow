#pragma once
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

struct SnContext;
typedef VALUE(*SnFunctionPtr)(struct SnContext*);

typedef uintx SnSymbol;
typedef int(*SnCompareFunc)(VALUE a, VALUE b);
typedef void(*SnPointerFreeFunc)(void* ptr);
struct SnContinuationInternal;
struct SnCodegenInternal;
struct SnVariableReferenceDescription;
struct SnVariableReference;
struct SnLock;
struct SnRWLock;

// the following hullaballoo is necessary because ISO C99 doesn't allow casts between data and function pointers. Snow does that a lot.
union function_pointer_caster_t {
	void* data;
	void(*function)();
};
typedef void(*basic_function_t)();
#define CAST_FUNCTION_TO_DATA(DATA, FUNCTION) union function_pointer_caster_t _caster ##__LINE__; _caster##__LINE__.function = (void(*)())(FUNCTION); *((void**)&(DATA)) = _caster##__LINE__.data
#define CAST_DATA_TO_FUNCTION(FUNCTION, DATA) union function_pointer_caster_t _caster ##__LINE__; _caster##__LINE__.data = (void*)(DATA); *((basic_function_t*)&(FUNCTION)) = _caster##__LINE__.function


enum SnImmediateType {
	kIntegerType = 0x1,
	kNil = 0x2,
	kFalse = 0x4,
	kTrue = 0x6,
	kSymbolType = 0xa,
#ifdef ARCH_IS_64_BIT
	// should be undefined in 32-bit, because there's not enough room for floats in 4-byte pointers.
	kFloatType = 0xe,
#endif
	kSpecialTypeMask = 0xe,
	
	kTypeMask = 0xf
};

#define SN_NIL ((VALUE)kNil)
#define SN_TRUE ((VALUE)kTrue)
#define SN_FALSE ((VALUE)kFalse)


#ifdef ARCH_IS_64_BIT
// Inline immediate floats -- only in 64-bit land.
// See Float.cpp for 32-bit implementations.
static inline bool snow_is_float(VALUE val) { return ((intx)val & kTypeMask) == kFloatType; }
static inline VALUE snow_float_to_value(float f) { return (VALUE)(((uintx)*((uint32_t*)&f) << 16) | kFloatType); }
static inline float snow_value_to_float(VALUE val) { uint32_t d = (uint32_t)((uintx)val >> 16); return *(float*)(&d); }
#endif

static inline bool snow_is_integer(VALUE val) { return (intx)val & 0x1; }
static inline bool snow_is_object(VALUE val) { return val && ((intx)val & kTypeMask) == 0; }
static inline bool snow_is_true(VALUE val) { return (intx)val == kTrue; }
static inline bool snow_is_false(VALUE val) { return (intx)val == kFalse; }
static inline bool snow_is_boolean(VALUE val) { return snow_is_true(val) || snow_is_false(val); }
static inline bool snow_is_nil(VALUE val) { return (intx)val == kNil; }
static inline bool snow_is_symbol(VALUE val) { return ((intx)val & kTypeMask) == kSymbolType; }
static inline bool snow_is_numeric(VALUE val) { return snow_is_integer(val) || snow_is_float(val); }

static inline VALUE snow_int_to_value(intx n) { return (VALUE)((n << 1) | 1); }
static inline intx snow_value_to_int(VALUE val) {
	#ifdef ARCH_IS_64_BIT
	return ((intx)val >> 1) | ((intx)val < 0 ? (intx)1 << 63 : 0);
	#else
	return ((intx)val >> 1) | ((intx)val < 0 ? (intx)1 << 31 : 0);
	#endif
}


static inline bool snow_eval_truth(VALUE val) {
	// WARNING: This is explicitly inlined in codegens. If you change this, you must also change each codegen that inlines this.
	return !(!val || val == SN_NIL || val == SN_FALSE);
}

static inline VALUE snow_boolean_to_value(bool b) { return b ? SN_TRUE : SN_FALSE; }
static inline bool snow_value_to_boolean(VALUE val) { return snow_eval_truth(val); }

static inline VALUE snow_symbol_to_value(SnSymbol sym) { return (VALUE)((sym << 4) | kSymbolType); }
static inline SnSymbol snow_value_to_symbol(VALUE val) { return (SnSymbol)val >> 4; }

typedef enum SnAstNodeTypess {
	/*
		IMPORTANT!
		When adding entries in this enum, remember to also add entries in ast.c for the
		desired size and name of the ast node.
	*/
	SN_AST_LITERAL,
	SN_AST_SEQUENCE,
	SN_AST_FUNCTION,
	SN_AST_RETURN,
	SN_AST_BREAK,
	SN_AST_CONTINUE,
	SN_AST_SELF,
	SN_AST_CURRENT_SCOPE,
	SN_AST_LOCAL,
	SN_AST_MEMBER,
	SN_AST_LOCAL_ASSIGNMENT,
	SN_AST_MEMBER_ASSIGNMENT,
	SN_AST_IF_ELSE,
	SN_AST_CALL,
	SN_AST_LOOP,
	SN_AST_TRY,
	SN_AST_CATCH,
	SN_AST_AND,
	SN_AST_OR,
	SN_AST_XOR,
	SN_AST_NOT,
	SN_AST_PARALLEL_THREAD,
	SN_AST_PARALLEL_FORK
} SnAstNodeTypes;


#if defined(__GNU_C__) && GCC_VERSION >= MAKE_VERSION(4, 4, 0)
#define ATTR_ALLOC_SIZE(...) __attribute__((alloc_size((__VA_ARGS__)),malloc))
#else
#define ATTR_ALLOC_SIZE(...)
#define ATTR_MALLOC
#endif

// Snow Malloc Interface
CAPI void* snow_malloc(uintx size)                            ATTR_ALLOC_SIZE(1);
CAPI void* snow_calloc(uintx count, uintx size)               ATTR_ALLOC_SIZE(2);
CAPI void* snow_realloc(void* ptr, uintx new_size)            ATTR_ALLOC_SIZE(2);
CAPI void* snow_malloc_aligned(uintx size, uintx alignment)   ATTR_ALLOC_SIZE(1);
CAPI void snow_free(void* ptr);

#endif /* end of include guard: BASIC_H_N5K8EFY5 */
