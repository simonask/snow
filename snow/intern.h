#ifndef INTERN_H_WXDJG2OI
#define INTERN_H_WXDJG2OI

#include "snow/basic.h"
#include "snow/value.h"
#include "snow/symbol.h"

#include <omp.h>

CAPI void warn(const char* msg, ...);
CAPI void error(const char* msg, ...);
#ifdef DEBUG
CAPI void debug(const char* msg, ...);
#else
CAPI inline void debug(const char*, ...) {}
#endif

#include <assert.h>

#define ASSERT assert
#define ASSERT_TYPE(val, type) ASSERT(is_object(val) && ((SnObjectBase*)val).type == type)

enum SnValueType {
	kObjectType = 0x0,
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
static inline bool is_object(VALUE val) { return val && ((intx)val & kTypeMask) == kObjectType; }
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

static inline VALUE symbol_to_value(SnSymbol sym) { return (VALUE)((sym << 0x10) | kSymbolType); }
static inline SnSymbol value_to_symbol(VALUE val) { return (SnSymbol)val >> 0x10; }

const char* value_to_string(VALUE val);

#endif /* end of include guard: INTERN_H_WXDJG2OI */
