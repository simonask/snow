#ifndef EXCEPTION_INTERN_H_XPB8M6RF
#define EXCEPTION_INTERN_H_XPB8M6RF

#include "snow/exception.h"
#include "snow/arch.h"

typedef struct SnExceptionHandler {
	struct SnExceptionHandler* previous;
	SnExecutionState state;
	VALUE exception;
} SnExceptionHandler;

CAPI SnExceptionHandler* snow_create_exception_handler();

#endif /* end of include guard: EXCEPTION_INTERN_H_XPB8M6RF */
