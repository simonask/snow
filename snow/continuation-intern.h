#ifndef CONTINUATION_INTERN_H_MHL4KUQW
#define CONTINUATION_INTERN_H_MHL4KUQW

#include "snow/basic.h"
#include "snow/continuation.h"

void _continuation_resume(SnContinuation* cc);
bool _continuation_save(SnContinuation* cc);
void _continuation_reset(SnContinuation* cc);

#endif /* end of include guard: CONTINUATION_INTERN_H_MHL4KUQW */
