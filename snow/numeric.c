#include "snow/numeric.h"
#include "snow/object.h"
#include "snow/intern.h"
#include "snow/context.h"
#include "snow/snow.h"
#include "snow/function.h"
#include "snow/str.h"

#include <stdio.h>

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

SnObject* create_integer_prototype()
{
	SnObject* proto = snow_create_object(NULL);
	snow_set_member(proto, snow_symbol("+"), snow_create_function(numeric_plus));
	snow_set_member(proto, snow_symbol("to_string"), snow_create_function(numeric_to_string));
	return proto;
}

SnObject* create_float_prototype()
{
	SnObject* proto = snow_create_object(NULL);
	return proto;
}