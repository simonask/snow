#include "snow/str.h"
#include "snow/gc.h"
#include "snow/linkbuffer.h"
#include "snow/intern.h"
#include <string.h>

SnString* snow_create_string(const char* cstr)
{
	STACK_GUARD;
	
	SnString* str = (SnString*)snow_alloc_any_object(SN_STRING_TYPE, sizeof(SnString));
	uintx len = strlen(cstr);
	str->str = snow_gc_alloc(len+1);
	memcpy(str->str, cstr, len+1);
	return str;
}

SnString* snow_create_string_from_data(const char* cstr, uintx len)
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

uintx snow_string_length(SnString* str)
{
	return strlen(str->str);
}

SnString* snow_string_concatenate(SnString* a, SnString* b)
{
	uintx len_a = snow_string_length(a);
	uintx len_b = snow_string_length(b);
	SnString* str = (SnString*)snow_alloc_any_object(SN_STRING_TYPE, sizeof(SnString));
	uintx len_result = len_a + len_b;
	str->str = snow_gc_alloc(len_result + 1);
	memcpy(str->str, a->str, len_a);
	memcpy(&str->str[len_a], b->str, len_b);
	str->str[len_result] = '\0';
	return str;
}

SNOW_FUNC(string_to_string)
{
	ASSERT_TYPE(SELF, SN_STRING_TYPE);
	return SELF;
}

SNOW_FUNC(string_inspect)
{
	ASSERT_TYPE(SELF, SN_STRING_TYPE);
	SnString* self = (SnString*)SELF;
	uintx len = snow_string_length(self);
	ASSERT_STACK_SPACE(len + 32);
	char str[len+3];
	memcpy(&str[1], self->str, len);
	str[0] = str[len+1] = '"';
	str[len+2] = '\0';
	return snow_create_string(str);
}

SNOW_FUNC(string_compare)
{
	ASSERT_TYPE(SELF, SN_STRING_TYPE);
	SnString* self = (SnString*)SELF;
	REQUIRE_ARGS(1);
	ASSERT_TYPE(ARGS[0], SN_STRING_TYPE);
	SnString* other = (SnString*)ARGS[0];
	return strcmp(self->str, other->str);
}

void init_string_class(SnClass* klass)
{
	snow_define_method_nocc(klass, "to_string", string_to_string);
	snow_define_method_nocc(klass, "inspect", string_inspect);
	snow_define_method_nocc(klass, "<=>", string_compare);
}
