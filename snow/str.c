#include "snow/str.h"
#include "snow/gc.h"
#include "snow/linkbuffer.h"
#include "snow/intern.h"
#include <string.h>
#include <stdlib.h>
#include <xlocale.h>

static inline size_t strlen_locale(const char* cstr_utf8, size_t num_bytes) {
	size_t length = 0;
	const char* p = cstr_utf8;
	while (num_bytes) {
		size_t char_size = mblen(p, num_bytes);
		++length;
		p += char_size;
		num_bytes -= char_size;
	}
	return length;
}

SnString* snow_create_string(const char* cstr_utf8)
{
	SnString* str = (SnString*)snow_alloc_any_object(SN_STRING_TYPE, sizeof(SnString));
	uintx len = strlen(cstr_utf8);
	str->data = snow_gc_alloc(len+1);
	memcpy(str->data, cstr_utf8, len+1);
	str->size = len;
	str->length = strlen_locale(str->data, len);
	return str;
}

SnString* snow_create_string_from_data(const byte* cstr_utf8, size_t num_bytes)
{
	SnString* str = (SnString*)snow_alloc_any_object(SN_STRING_TYPE, sizeof(SnString));
	str->data = snow_gc_alloc(num_bytes+1);
	memcpy(str->data, cstr_utf8, num_bytes);
	str->data[num_bytes] = '\0';
	str->size = num_bytes;
	str->length = strlen_locale(str->data, num_bytes);
	return str;
}

SnString* snow_create_string_from_linkbuffer(SnLinkBuffer* buffer)
{
	SnString* str = (SnString*)snow_alloc_any_object(SN_STRING_TYPE, sizeof(SnString));
	uintx len = snow_linkbuffer_size(buffer);
	str->data = snow_gc_alloc(len + 1);
	snow_linkbuffer_copy_data(buffer, str->data, len);
	str->data[len] = '\0';
	str->size = len;
	str->length = strlen_locale(str->data, len);
	return str;
}

SnString* snow_format_string(const char* format, ...)
{
	size_t num_insertions = 0;
	const char* cp = format;
	while (*cp) {
		if (*cp == '%' && *(cp+1) == '@') { ++num_insertions; ++cp; }
		++cp;
	}
	size_t len = cp - format;
	
	SnString* insertions[num_insertions];
	size_t combined_insertions_len = 0;
	va_list ap;
	va_start(ap, format);
	for (size_t i = 0; i < num_insertions; ++i) {
		VALUE object = va_arg(ap, VALUE);
		insertions[i] = snow_call_method(object, snow_symbol("to_string"), 0);
		combined_insertions_len += snow_string_size(insertions[i]);
	}
	va_end(ap);
	
	size_t result_len = (len - (num_insertions*2)) + combined_insertions_len;
	char buffer[result_len + 1];
	cp = format;
	size_t j = 0;
	for (size_t i = 0; i < result_len;) {
		if (*cp == '%' && *(cp+1) == '@') {
			memcpy(buffer + i, snow_string_cstr(insertions[j]), snow_string_size(insertions[j]));
			i += snow_string_size(insertions[j]);
			++j;
			++cp;
		} else {
			buffer[i++] = *cp;
		}
		++cp;
	}
	buffer[result_len] = '\0';
	
	return snow_create_string_from_data(buffer, result_len);
}

intx snow_string_compare(SnString* a, SnString* b)
{
	return strcmp(a->data, b->data);
}

SnString* snow_string_concatenate(SnString* a, SnString* b)
{
	size_t len_a = snow_string_size(a);
	size_t len_b = snow_string_size(b);
	SnString* str = (SnString*)snow_alloc_any_object(SN_STRING_TYPE, sizeof(SnString));
	uintx len_result = len_a + len_b;
	str->data = snow_gc_alloc(len_result + 1);
	memcpy(str->data, a->data, len_a);
	memcpy(&str->data[len_a], b->data, len_b);
	str->data[len_result] = '\0';
	str->size = len_result;
	str->length = strlen_locale(str->data, len_result);
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
	uintx len = snow_string_size(self);
	char str[len+3];
	memcpy(&str[1], self->data, len);
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
	return int_to_value(snow_string_compare(self, other));
}

SNOW_FUNC(string_size)
{
	ASSERT_TYPE(SELF, SN_STRING_TYPE);
	SnString* self = (SnString*)SELF;
	return int_to_value(snow_string_size(self));
}

SNOW_FUNC(string_length)
{
	ASSERT_TYPE(SELF, SN_STRING_TYPE);
	SnString* self = (SnString*)SELF;
	return int_to_value(snow_string_length(self));
}

SNOW_FUNC(string_concatenate)
{
	ASSERT_TYPE(SELF, SN_STRING_TYPE);
	REQUIRE_ARGS(1);
	SnString* self = (SnString*)SELF;
	SnString* other = (SnString*)ARGS[0];
	ASSERT_TYPE(other, SN_STRING_TYPE);
	return snow_string_concatenate(self, other);
}

void init_string_class(SnClass* klass)
{
	snow_define_method(klass, "to_string", string_to_string);
	snow_define_method(klass, "inspect", string_inspect);
	snow_define_method(klass, "<=>", string_compare);
	snow_define_property(klass, "size", string_size, NULL);
	snow_define_property(klass, "length", string_length, NULL);
	snow_define_method(klass, "+", string_concatenate);
}
