#include "snow/intern.h"
#include "snow/str.h"

SNOW_FUNC(nil_to_string) {
	return snow_create_string("");
}

SNOW_FUNC(nil_inspect) {
	return snow_create_string("nil");
}

SNOW_FUNC(nil_any) {
	return SN_FALSE;
}

void init_nil_class(SnClass* klass)
{
	snow_define_method(klass, "to_string", nil_to_string);
	snow_define_method(klass, "inspect", nil_inspect);
	snow_define_property(klass, "any?", nil_any, NULL);
}