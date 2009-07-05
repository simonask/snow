#include "snow/intern.h"
#include <stdio.h>
#include <stdarg.h>

void warn(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vaprintf(fmt, ap);
	va_end(ap);
}

void error(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vaprintf(fmt, ap);
	va_end(ap);
}

void debug(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vaprintf(fmt, ap);
	va_end(ap);
}

