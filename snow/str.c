#include "snow/str.h"
#include "snow/gc.h"
#include <string.h>

SnString* snow_create_string(const char* cstr)
{
	SnString* str = (SnString*)snow_alloc_any_object(SN_STRING_TYPE, sizeof(SnString));
	uintx len = strlen(cstr);
	str->str = snow_gc_alloc(len+1);
	memcpy(str->str, cstr, len+1);
	return str;
}