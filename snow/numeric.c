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
	ASSERT(is_numeric(SELF));
	REQUIRE_ARGS(1);
	ASSERT(is_numeric(ARGS[0]));
	
	if (is_integer(SELF) && is_integer(ARGS[0])) {
		return int_to_value(value_to_int(SELF) + value_to_int(ARGS[0]));
	} else {
		float a = is_integer(SELF) ? (float)value_to_int(SELF) : value_to_float(SELF);
		float b = is_integer(ARGS[0]) ? (float)value_to_int(ARGS[0]) : value_to_float(ARGS[0]);
		return float_to_value(a + b);
	}
}

SNOW_FUNC(numeric_minus) {
	ASSERT(is_numeric(SELF));
	
	if (NUM_ARGS >= 1)
	{
		ASSERT(is_numeric(ARGS[0]));
		if (is_integer(SELF) && is_integer(ARGS[0])) {
			return int_to_value(value_to_int(SELF) - value_to_int(ARGS[0]));
		} else {
			float a = is_integer(SELF) ? (float)value_to_int(SELF) : value_to_float(SELF);
			float b = is_integer(ARGS[0]) ? (float)value_to_int(ARGS[0]) : value_to_float(ARGS[0]);
			return float_to_value(a - b);
		}
	}
	else
	{
		if (is_integer(SELF)) return int_to_value(-value_to_int(SELF));
		else return float_to_value(-value_to_float(SELF));
	}
}

SNOW_FUNC(numeric_multiply) {
	ASSERT(is_numeric(SELF));
	REQUIRE_ARGS(1);
	ASSERT(is_numeric(ARGS[0]));
	
	if (is_integer(SELF) && is_integer(ARGS[0])) {
		return int_to_value(value_to_int(SELF) * value_to_int(ARGS[0]));
	} else {
		float a = is_integer(SELF) ? (float)value_to_int(SELF) : value_to_float(SELF);
		float b = is_integer(ARGS[0]) ? (float)value_to_int(ARGS[0]) : value_to_float(ARGS[0]);
		return float_to_value(a * b);
	}
}

SNOW_FUNC(numeric_divide) {
	ASSERT(is_numeric(SELF));
	REQUIRE_ARGS(1);
	ASSERT(is_numeric(ARGS[0]));
	
	if (is_integer(SELF) && is_integer(ARGS[0])) {
		return int_to_value(value_to_int(SELF) / value_to_int(ARGS[0]));
	} else {
		float a = is_integer(SELF) ? (float)value_to_int(SELF) : value_to_float(SELF);
		float b = is_integer(ARGS[0]) ? (float)value_to_int(ARGS[0]) : value_to_float(ARGS[0]);
		return float_to_value(a / b);
	}
}

SNOW_FUNC(numeric_modulo) {
	ASSERT(is_numeric(SELF));
	REQUIRE_ARGS(1);
	ASSERT(is_numeric(ARGS[0]));
	
	if (is_integer(SELF) && is_integer(ARGS[0])) {
		return int_to_value(value_to_int(SELF) % value_to_int(ARGS[0]));
	} else {
		float a = is_integer(SELF) ? (float)value_to_int(SELF) : value_to_float(SELF);
		float b = is_integer(ARGS[0]) ? (float)value_to_int(ARGS[0]) : value_to_float(ARGS[0]);
		return float_to_value(fmodf(a, b));
	}
}

SNOW_FUNC(numeric_power) {
	ASSERT(is_numeric(SELF));
	REQUIRE_ARGS(1);
	ASSERT(is_numeric(ARGS[0]));
	
	if (is_integer(SELF) && is_integer(ARGS[0])) {
		return int_to_value((intx)pow((double)value_to_int(SELF), (double)value_to_int(ARGS[0])));
	} else {
		float a = is_integer(SELF) ? (float)value_to_int(SELF) : value_to_float(SELF);
		float b = is_integer(ARGS[0]) ? (float)value_to_int(ARGS[0]) : value_to_float(ARGS[0]);
		return float_to_value(powf(a, b));
	}
}

SNOW_FUNC(numeric_less_than) {
	ASSERT(is_numeric(SELF));
	REQUIRE_ARGS(1);
	ASSERT(is_numeric(ARGS[0]));
	
	if (is_integer(SELF) && is_integer(ARGS[0])) {
		return boolean_to_value(value_to_int(SELF) < value_to_int(ARGS[0]));
	} else {
		float a = is_integer(SELF) ? (float)value_to_int(SELF) : value_to_float(SELF);
		float b = is_integer(ARGS[0]) ? (float)value_to_int(ARGS[0]) : value_to_float(ARGS[0]);
		return boolean_to_value(a < b);
	}
}

SNOW_FUNC(numeric_greater_than) {
	ASSERT(is_numeric(SELF));
	REQUIRE_ARGS(1);
	ASSERT(is_numeric(ARGS[0]));
	
	if (is_integer(SELF) && is_integer(ARGS[0])) {
		return boolean_to_value(value_to_int(SELF) > value_to_int(ARGS[0]));
	} else {
		float a = is_integer(SELF) ? (float)value_to_int(SELF) : value_to_float(SELF);
		float b = is_integer(ARGS[0]) ? (float)value_to_int(ARGS[0]) : value_to_float(ARGS[0]);
		return boolean_to_value(a > b);
	}
}

SNOW_FUNC(numeric_to_string) {
	ASSERT(is_numeric(SELF));
	
	char r[32];	// should be enough to hold all 64-bit ints and 32-bit floats
	
	if (is_integer(SELF)) {
		intx n = value_to_int(SELF);
		snprintf(r, 32, "%lld", n);
	} else if (is_float(SELF)) {
		float f = value_to_float(SELF);
		snprintf(r, 32, "%f", f);
	}
	else
		TRAP(); // not integer or float, but numeric? srsly dude?
	
	return snow_create_string(r);
}

void init_integer_class(SnClass* klass)
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

void init_float_class(SnClass* klass)
{
	
}