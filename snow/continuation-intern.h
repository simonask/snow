#ifndef CONTINUATION_INTERN_H_MHL4KUQW
#define CONTINUATION_INTERN_H_MHL4KUQW

#include "snow/basic.h"
#include "snow/continuation.h"

HIDDEN __attribute__((noreturn)) void _continuation_resume(SnContinuation* cc)           ATTR_HOT;
HIDDEN __attribute__((returns_twice)) bool _continuation_save(SnContinuation* cc)        ATTR_HOT;
HIDDEN void _continuation_reset(SnContinuation* cc)                                      ATTR_HOT;

#endif /* end of include guard: CONTINUATION_INTERN_H_MHL4KUQW */
