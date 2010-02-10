#include "test/test.h"
#include "snow/intern.h"
#include "snow/exception.h"
#include "snow/exception-intern.h"
#include "snow/task-intern.h"

TEST_CASE(no_throwing) {
	SnExceptionHandler* starting_handler = snow_current_exception_handler();
	bool tried = false, ensured = false;
	
	SnTryState state;
	switch (snow_begin_try(&state)) {
		case SnTryResumptionStateTrying:
			tried = true;
			break;
		case SnTryResumptionStateCatching:
			TEST(!"Catch block executed!")
			break;
		case SnTryResumptionStateEnsuring:
			ensured = true;
			break;
	}
	snow_end_try(&state);
	
	TEST(tried);
	TEST(ensured);
	TEST_EQ(starting_handler, snow_current_exception_handler());
}

TEST_CASE(throws_once) {
	SnExceptionHandler* starting_handler = snow_current_exception_handler();
	bool tried = false, caught = false, ensured = false;
	
	VALUE exception = int_to_value(60);
	
	SnTryState state;
	switch (snow_begin_try(&state)) {
		case SnTryResumptionStateTrying:
			tried = true;
			snow_throw_exception(int_to_value(60));
			TEST(!"Should never be reached!");
		case SnTryResumptionStateCatching:
			caught = true;
			TEST(snow_current_exception() == exception);
			break;
		case SnTryResumptionStateEnsuring:
			ensured = true;
			TEST(snow_current_exception() == exception);
			break;
	}
	snow_end_try(&state);
	
	TEST(tried);
	TEST(caught);
	TEST(ensured);
	TEST_EQ(starting_handler, snow_current_exception_handler());
}

TEST_CASE(throws_in_catch) {
	SnExceptionHandler* starting_handler = snow_current_exception_handler();
	bool tried = false, caught = false, ensured = false;
	VALUE exception_thrown = NULL;
	
	SnTryState wrapper, state;
	switch (snow_begin_try(&wrapper)) {
		case SnTryResumptionStateTrying:
			switch (snow_begin_try(&state)) {
				case SnTryResumptionStateTrying:
					tried = true;
					snow_throw_exception(int_to_value(60));
				case SnTryResumptionStateCatching:
					caught = true;
					snow_throw_exception(int_to_value(80));
				case SnTryResumptionStateEnsuring:
					ensured = true;
					break;
			}
			snow_end_try(&state);
			FAIL("snow_end_try didn't propagate!");
			
			break;
		case SnTryResumptionStateCatching:
			exception_thrown = snow_current_exception();
			break;
		default: break;
	}
	snow_end_try(&wrapper);
	
	TEST(tried);
	TEST(caught);
	TEST(ensured);
	TEST_EQ(exception_thrown, int_to_value(80));
	TEST_EQ(starting_handler, snow_current_exception_handler());
}

TEST_CASE(throws_in_ensure) {
	SnExceptionHandler* starting_handler = snow_current_exception_handler();
	VALUE exception = NULL;
	
	SnTryState wrapper, state;
	switch (snow_begin_try(&wrapper)) {
		case SnTryResumptionStateTrying:
			switch (snow_begin_try(&state)) {
				case SnTryResumptionStateEnsuring:
					snow_throw_exception(int_to_value(60));
				
				default: break;
			}
			snow_end_try(&state);
			TEST(!"snow_end_try didn't propagate!");
			
			break;
		case SnTryResumptionStateCatching:
			exception = snow_current_exception();
			break;
		default: break;
	}
	snow_end_try(&wrapper);
	
	TEST_EQ(exception, int_to_value(60));
	TEST_EQ(starting_handler, snow_current_exception_handler());
}
