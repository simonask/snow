#include "snow/intern.h"

SNOW_FUNC(boolean_to_string) {
	if (snow_eval_truth(SELF)) {
		return snow_create_string("true");
	}
	return snow_create_string("false");
}

void init_boolean_class(SnClass* klass)
{
	snow_define_method(klass, "to_string", boolean_to_string);
	snow_define_method(klass, "inspect", boolean_to_string);
}
