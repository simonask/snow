#ifndef EXCEPTION_H_9D78UTT5
#define EXCEPTION_H_9D78UTT5

#include "snow/object.h"
#include "snow/arch.h"

typedef void(*SnExceptionTryFunc)(void* userdata);
typedef void(*SnExceptionCatchFunc)(VALUE exception, void* userdata);
typedef void(*SnExceptionEnsureFunc)(void* userdata);

struct SnString;
struct SnContinuation;

typedef struct SnException {
	SnObject base;
	struct SnString* description;
	struct SnContinuation* thrown_by;
} SnException;

CAPI void snow_throw_exception(VALUE exception);
CAPI void snow_throw_exception_with_description(const char* description, ...);
CAPI void snow_try_catch_ensure(SnExceptionTryFunc try_func, SnExceptionCatchFunc catch_func, SnExceptionEnsureFunc ensure_func, void* userdata);

CAPI SnException* snow_current_exception();

CAPI SnException* snow_create_exception();
CAPI SnException* snow_create_exception_with_description(const char* description);

typedef enum SnTryResumptionState {
	SnTryResumptionStateTrying,
	SnTryResumptionStateCatching,
	SnTryResumptionStateEnsuring
} SnTryResumptionState;

typedef struct SnTryState {
	bool started;
	bool iteration_was_prepared;
	SnTryResumptionState resumption_state;
	SnException* exception_to_propagate;
} SnTryState;

#define snow_begin_try(state) (snow_save_execution_state(snow_try_setup_resumptions(state)), snow_try_prepare_resumption_iteration(state))
CAPI void snow_end_try(SnTryState* state);
CAPI SnExecutionState* snow_try_setup_resumptions(SnTryState* state);
CAPI SnTryResumptionState snow_try_prepare_resumption_iteration(SnTryState* state);

#endif /* end of include guard: EXCEPTION_H_9D78UTT5 */
