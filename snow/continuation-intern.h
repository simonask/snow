#ifndef CONTINUATION_INTERN_H_MHL4KUQW
#define CONTINUATION_INTERN_H_MHL4KUQW

#include "snow/basic.h"
#include "snow/continuation.h"

HIDDEN __attribute__((noreturn)) void _continuation_resume(SnContinuation* cc);
HIDDEN __attribute__((returns_twice)) bool _continuation_save(SnContinuation* cc);
HIDDEN void _continuation_reset(SnContinuation* cc);

#endif /* end of include guard: CONTINUATION_INTERN_H_MHL4KUQW */
