#ifndef STRING_H_OYTL2E1P
#define STRING_H_OYTL2E1P

#include "snow/basic.h"
#include "snow/object.h"

typedef struct SnString {
	SnObjectBase base;
	char* data;
	uint32_t size;   // bytes
	uint32_t length; // characters
} SnString;

struct SnLinkBuffer;

CAPI SnString* snow_create_string(const char* cstr_utf8);
CAPI SnString* snow_create_string_from_data(const byte* start, size_t num_bytes);
CAPI SnString* snow_create_string_from_linkbuffer(struct SnLinkBuffer* buf);
CAPI SnString* snow_format_string(const char* cstr_utf8, ...);       // expects each argument to be a VALUE, and calls to_string on each. "foo %@ bar %@"
CAPI intx snow_string_compare(SnString* a, SnString* b);
static inline uint32_t snow_string_length(SnString* str) { return str->length; }
static inline uint32_t snow_string_size(SnString* str) { return str->size; }
CAPI SnString* snow_string_concatenate(SnString* a, SnString* b);
static inline const char* snow_string_cstr(SnString* str) { return str->data; }
static inline char snow_string_byte_at(SnString* str, size_t idx) { return str->data[idx]; }
CAPI void snow_string_character_at(SnString* str, size_t idx, char** out_char, size_t* out_length);

#endif /* end of include guard: STRING_H_OYTL2E1P */
