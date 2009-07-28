#include "snow/str.h"
#include "snow/gc.h"
#include "snow/linkbuffer.h"
#include <string.h>

SnString* snow_create_string(const char* cstr)
{
	SnString* str = (SnString*)snow_alloc_any_object(SN_STRING_TYPE, sizeof(SnString));
	uintx len = strlen(cstr);
	str->str = snow_gc_alloc(len+1);
	memcpy(str->str, cstr, len+1);
	return str;
}

SnString* snow_create_string_from_Data(const char* cstr, uintx len)
{
	SnString* str = (SnString*)snow_alloc_any_object(SN_STRING_TYPE, sizeof(SnString));
	str->str = snow_gc_alloc(len+1);
	memcpy(str->str, cstr, len);
	str->str[len] = '\0';
	return str;
}

SnString* snow_create_string_from_linkbuffer(SnLinkBuffer* buffer)
{
	SnString* str = (SnString*)snow_alloc_any_object(SN_STRING_TYPE, sizeof(SnString));
	uintx len = snow_linkbuffer_size(buffer);
	str->str = snow_gc_alloc(len + 1);
	snow_linkbuffer_copy_data(buffer, str->str, len);
	str->str[len] = '\0';
	return str;
}

intx snow_string_compare(SnString* a, SnString* b)
{
	return strcmp(a->str, b->str);
}

SnObject* create_string_prototype()
{
	SnObject* proto = snow_create_object(NULL);
	return proto;
}