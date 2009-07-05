#include "snow/string.h"
#include <string.h>

SnString* snow_create_string(const char* str)
{
	SnString* str = snow_alloc_any_object(SN_STRING_TYPE, sizeof(SnString));
	uintx len = strlen(str);
	str->str = snow_gc_alloc(len+1);
	memcpy(str->str, str, len+1);
	return str;
}