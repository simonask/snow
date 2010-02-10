#include "snow/intern.h"
#include "snow/str.h"
#include "snow/snow.h"
#include <stdio.h>
#include <stdarg.h>

void warn(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

void error(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

#if defined(DEBUG)
void debug(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}
#endif

const char* value_to_cstr(VALUE val) {
	VALUE converted = snow_call_method(val, snow_symbol("to_string"), 0);
	SnString* str = (SnString*)converted;
	ASSERT(str->base.type == SN_STRING_TYPE);
	return snow_string_cstr(str);
}
