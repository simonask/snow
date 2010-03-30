#include "snow/numeric.h"
#include "snow/object.h"
#include "snow/intern.h"
#include "snow/context.h"
#include "snow/snow.h"
#include "snow/function.h"
#include "snow/str.h"

#include <stdio.h>
#include <math.h>

SNOW_FUNC(numeric_plus) {
	ASSERT(snow_is_numeric(SELF));
	REQUIRE_ARGS(1);
	ASSERT(snow_is_numeric(ARGS[0]));
	
	if (snow_is_integer(SELF) && snow_is_integer(ARGS[0])) {
		return snow_int_to_value(snow_value_to_int(SELF) + snow_value_to_int(ARGS[0]));
	} else {
		float a = snow_is_integer(SELF) ? (float)snow_value_to_int(SELF) : snow_value_to_float(SELF);
		float b = snow_is_integer(ARGS[0]) ? (float)snow_value_to_int(ARGS[0]) : snow_value_to_float(ARGS[0]);
		return snow_float_to_value(a + b);
	}
}

SNOW_FUNC(numeric_minus) {
	ASSERT(snow_is_numeric(SELF));
	
	if (NUM_ARGS >= 1)
	{
		ASSERT(snow_is_numeric(ARGS[0]));
		if (snow_is_integer(SELF) && snow_is_integer(ARGS[0])) {
			return snow_int_to_value(snow_value_to_int(SELF) - snow_value_to_int(ARGS[0]));
		} else {
			float a = snow_is_integer(SELF) ? (float)snow_value_to_int(SELF) : snow_value_to_float(SELF);
			float b = snow_is_integer(ARGS[0]) ? (float)snow_value_to_int(ARGS[0]) : snow_value_to_float(ARGS[0]);
			return snow_float_to_value(a - b);
		}
	}
	else
	{
		if (snow_is_integer(SELF)) return snow_int_to_value(-snow_value_to_int(SELF));
		else return snow_float_to_value(-snow_value_to_float(SELF));
	}
}

SNOW_FUNC(numeric_multiply) {
	ASSERT(snow_is_numeric(SELF));
	REQUIRE_ARGS(1);
	ASSERT(snow_is_numeric(ARGS[0]));
	
	if (snow_is_integer(SELF) && snow_is_integer(ARGS[0])) {
		return snow_int_to_value(snow_value_to_int(SELF) * snow_value_to_int(ARGS[0]));
	} else {
		float a = snow_is_integer(SELF) ? (float)snow_value_to_int(SELF) : snow_value_to_float(SELF);
		float b = snow_is_integer(ARGS[0]) ? (float)snow_value_to_int(ARGS[0]) : snow_value_to_float(ARGS[0]);
		return snow_float_to_value(a * b);
	}
}

SNOW_FUNC(numeric_divide) {
	ASSERT(snow_is_numeric(SELF));
	REQUIRE_ARGS(1);
	ASSERT(snow_is_numeric(ARGS[0]));
	
	if (snow_is_integer(SELF) && snow_is_integer(ARGS[0])) {
		return snow_int_to_value(snow_value_to_int(SELF) / snow_value_to_int(ARGS[0]));
	} else {
		float a = snow_is_integer(SELF) ? (float)snow_value_to_int(SELF) : snow_value_to_float(SELF);
		float b = snow_is_integer(ARGS[0]) ? (float)snow_value_to_int(ARGS[0]) : snow_value_to_float(ARGS[0]);
		return snow_float_to_value(a / b);
	}
}

SNOW_FUNC(numeric_modulo) {
	ASSERT(snow_is_numeric(SELF));
	REQUIRE_ARGS(1);
	ASSERT(snow_is_numeric(ARGS[0]));
	
	if (snow_is_integer(SELF) && snow_is_integer(ARGS[0])) {
		return snow_int_to_value(snow_value_to_int(SELF) % snow_value_to_int(ARGS[0]));
	} else {
		float a = snow_is_integer(SELF) ? (float)snow_value_to_int(SELF) : snow_value_to_float(SELF);
		float b = snow_is_integer(ARGS[0]) ? (float)snow_value_to_int(ARGS[0]) : snow_value_to_float(ARGS[0]);
		return snow_float_to_value(fmodf(a, b));
	}
}

SNOW_FUNC(numeric_power) {
	ASSERT(snow_is_numeric(SELF));
	REQUIRE_ARGS(1);
	ASSERT(snow_is_numeric(ARGS[0]));
	
	if (snow_is_integer(SELF) && snow_is_integer(ARGS[0])) {
		return snow_int_to_value((intx)pow((double)snow_value_to_int(SELF), (double)snow_value_to_int(ARGS[0])));
	} else {
		float a = snow_is_integer(SELF) ? (float)snow_value_to_int(SELF) : snow_value_to_float(SELF);
		float b = snow_is_integer(ARGS[0]) ? (float)snow_value_to_int(ARGS[0]) : snow_value_to_float(ARGS[0]);
		return snow_float_to_value(powf(a, b));
	}
}

SNOW_FUNC(numeric_less_than) {
	ASSERT(snow_is_numeric(SELF));
	REQUIRE_ARGS(1);
	ASSERT(snow_is_numeric(ARGS[0]));
	
	if (snow_is_integer(SELF) && snow_is_integer(ARGS[0])) {
		return snow_boolean_to_value(snow_value_to_int(SELF) < snow_value_to_int(ARGS[0]));
	} else {
		float a = snow_is_integer(SELF) ? (float)snow_value_to_int(SELF) : snow_value_to_float(SELF);
		float b = snow_is_integer(ARGS[0]) ? (float)snow_value_to_int(ARGS[0]) : snow_value_to_float(ARGS[0]);
		return snow_boolean_to_value(a < b);
	}
}

SNOW_FUNC(numeric_greater_than) {
	ASSERT(snow_is_numeric(SELF));
	REQUIRE_ARGS(1);
	ASSERT(snow_is_numeric(ARGS[0]));
	
	if (snow_is_integer(SELF) && snow_is_integer(ARGS[0])) {
		return snow_boolean_to_value(snow_value_to_int(SELF) > snow_value_to_int(ARGS[0]));
	} else {
		float a = snow_is_integer(SELF) ? (float)snow_value_to_int(SELF) : snow_value_to_float(SELF);
		float b = snow_is_integer(ARGS[0]) ? (float)snow_value_to_int(ARGS[0]) : snow_value_to_float(ARGS[0]);
		return snow_boolean_to_value(a > b);
	}
}

SNOW_FUNC(numeric_to_string) {
	ASSERT(snow_is_numeric(SELF));
	
	char r[32];	// should be enough to hold all 64-bit ints and 32-bit floats
	
	if (snow_is_integer(SELF)) {
		intx n = snow_value_to_int(SELF);
		#ifdef ARCH_IS_64_BIT
		snprintf(r, 32, "%lld", n);
		#else
		snprintf(r, 32, "%d", n);
		#endif
	} else if (snow_is_float(SELF)) {
		float f = snow_value_to_float(SELF);
		snprintf(r, 32, "%f", f);
	}
	else
		TRAP(); // not integer or float, but numeric? srsly dude?
	
	return snow_create_string(r);
}

void SnInteger_init_class(SnClass* klass)
{
	snow_define_method(klass, "+", numeric_plus);
	snow_define_method(klass, "-", numeric_minus);
	snow_define_method(klass, "*", numeric_multiply);
	snow_define_method(klass, "/", numeric_divide);
	snow_define_method(klass, "%", numeric_modulo);
	snow_define_method(klass, "**", numeric_power);
	snow_define_method(klass, "<", numeric_less_than);
	snow_define_method(klass, ">", numeric_greater_than);
	snow_define_method(klass, "to_string", numeric_to_string);
	snow_define_method(klass, "inspect", numeric_to_string);
}

void SnFloat_init_class(SnClass* klass)
{
	
}