#ifndef INTERN_H_WXDJG2OI
#define INTERN_H_WXDJG2OI

#if defined(__GNUC__) && __INCLUDE_LEVEL__ != 1
#error Please don't include intern.h in header files.
#endif

#include "snow/snow.h"
#include "snow/basic.h"
#include "snow/symbol.h"
#include "snow/arch.h"
#include "snow/object.h"
#include "snow/class.h"
#include "snow/continuation.h"
#include "snow/gc.h"
#include "snow/exception.h"

CAPI void warn(const char* msg, ...);
CAPI void error(const char* msg, ...);
#ifdef DEBUG
CAPI void debug(const char* msg, ...);
#else
CAPI inline void debug(const char* msg, ...) {}
#endif

#define _QUOTEME(x) #x
#define Q(x) _QUOTEME(x)

#define ASSERT(x) if (!(x)) TRAP();
#define ASSERT_TYPE(OBJECT, TYPE) ASSERT(snow_typeof(OBJECT) == (TYPE))

enum SnValueType {
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
static inline bool is_float(VALUE val) { return ((intx)val & kTypeMask) == kFloatType; }
static inline VALUE float_to_value(float f) { return (VALUE)(((uintx)*((uint32_t*)&f) << 16) | kFloatType); }
static inline float value_to_float(VALUE val) { uint32_t d = (uint32_t)((uintx)val >> 16); return *(float*)(&d); }
#endif

static inline bool is_integer(VALUE val) { return (intx)val & 0x1; }
static inline bool is_object(VALUE val) { return val && ((intx)val & kTypeMask) == 0; }
static inline bool is_true(VALUE val) { return (intx)val == kTrue; }
static inline bool is_false(VALUE val) { return (intx)val == kFalse; }
static inline bool is_boolean(VALUE val) { return is_true(val) || is_false(val); }
static inline bool is_nil(VALUE val) { return (intx)val == kNil; }
static inline bool is_symbol(VALUE val) { return ((intx)val & kTypeMask) == kSymbolType; }
static inline bool is_numeric(VALUE val) { return is_integer(val) || is_float(val); }

static inline VALUE int_to_value(intx n) { return (VALUE)((n << 1) | 1); }
static inline intx value_to_int(VALUE val) {
	#ifdef ARCH_IS_64_BIT
	return ((intx)val >> 1) | ((intx)val < 0 ? (intx)1 << 63 : 0);
	#else
	return ((intx)val >> 1) | ((intx)val < 0 ? (intx)1 << 31 : 0);
	#endif
}

static inline VALUE boolean_to_value(bool b) { return b ? SN_TRUE : SN_FALSE; }
static inline bool value_to_boolean(VALUE val) { return snow_eval_truth(val); }

static inline VALUE symbol_to_value(SnSymbol sym) { return (VALUE)((sym << 4) | kSymbolType); }
static inline SnSymbol value_to_symbol(VALUE val) { return (SnSymbol)val >> 4; }

const char* value_to_cstr(VALUE val);

static inline SnObjectType snow_typeof(VALUE val)
{
	if (is_object(val)) return ((SnObjectBase*)val)->type;
	if (is_integer(val)) return SN_INTEGER_TYPE;
	if (is_nil(val) || !val) return SN_NIL_TYPE;
	if (is_boolean(val)) return SN_BOOLEAN_TYPE;
	if (is_symbol(val)) return SN_SYMBOL_TYPE;
	if (is_float(val)) return SN_FLOAT_TYPE;
	TRAP(); // unknown type?
	return 0;
}


// API convenience

#define SNOW_FUNC(NAME) static VALUE NAME(SnContext* _context)
#define SELF (_context->self)
#define ARGS (_context->args->data.data)
#define ARG_BY_SYM_AT(SYM, EXPECTED_POSITION) (_context->args ? snow_arguments_get_by_name_at(_context->args, (SYM), (EXPECTED_POSITION)) : NULL)
#define ARG_BY_NAME_AT(NAME, EXPECTED_POSITION) ARG_BY_SYM_AT(snow_symbol(NAME), (EXPECTED_POSITION))
#define ARG_BY_SYM(SYM) (_context->args ? (snow_arguments_get_by_name(_context->args, (SYM))) : NULL)
#define ARG_BY_NAME(NAME) ARG_BY_SYM(snow_symbol(NAME))
#define NUM_ARGS (_context->args ? _context->args->data.size : 0)
#define REQUIRE_ARGS(N) do { if (_context->args->data.size < (N)) snow_throw_exception_with_description("Expected %d argument%s for function call.", (N), (N) == 1 ? "" : "s"); } while (0)

// place this macro in all recursive C functions
#define STACK_GUARD snow_continuation_stack_guard()
// place this with large stack allocations
#define ASSERT_STACK_SPACE(n) ASSERT(snow_current_continuation_available_stack_space() >= n);

// Implementations that may depend on the definitions above
#include "snow/lock-impl.h"

#endif /* end of include guard: INTERN_H_WXDJG2OI */
