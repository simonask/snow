#include "snow/intern.h"
#include "snow/string.h"
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

void debug(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

const char* value_to_string(VALUE val) {
	VALUE converted = snow_call_method(val, snow_symbol("to_string"), 0);
	SnString* str = (SnString*)converted;
	ASSERT(str->base.type == SN_STRING_TYPE);
	return str->str;
}
