#ifndef EXCEPTION_H_9D78UTT5
#define EXCEPTION_H_9D78UTT5

#include "snow/object.h"

typedef void(*SnExceptionTryFunc)(void* userdata);
typedef void(*SnExceptionCatchFunc)(VALUE exception, void* userdata);
typedef void(*SnExceptionEnsureFunc)(void* userdata);

struct SnString;
struct SnContinuation;

CAPI void snow_throw_exception(VALUE exception);
CAPI void snow_throw_exception_with_description(const char* description, ...);
CAPI void snow_try_catch_ensure(SnExceptionTryFunc try_func, SnExceptionCatchFunc catch_func, SnExceptionEnsureFunc ensure_func, void* userdata);

typedef struct SnException {
	SnObject base;
	struct SnString* description;
	struct SnContinuation* thrown_by;
} SnException;

CAPI SnException* snow_create_exception();
CAPI SnException* snow_create_exception_with_description(const char* description);

#endif /* end of include guard: EXCEPTION_H_9D78UTT5 */
