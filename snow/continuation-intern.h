#ifndef CONTINUATION_INTERN_H_MHL4KUQW
#define CONTINUATION_INTERN_H_MHL4KUQW

#include "snow/basic.h"
#include "snow/continuation.h"
#include "snow/arch.h"

typedef struct SnContinuationInternal
{
	byte* stack_hi;
	byte* stack_lo;
	SnExecutionState state;
	bool running;
} SnContinuationInternal;

HIDDEN /*__attribute__((noreturn))  DISABLED BECAUSE GCC IS BUGGY */ void _continuation_resume(SnContinuation* cc)           ATTR_HOT;
HIDDEN void _continuation_reset(SnContinuation* cc)                                      ATTR_HOT;

#endif /* end of include guard: CONTINUATION_INTERN_H_MHL4KUQW */
