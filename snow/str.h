#ifndef STRING_H_OYTL2E1P
#define STRING_H_OYTL2E1P

#include "snow/basic.h"
#include "snow/object.h"

typedef struct SnString {
	SnObjectBase base;
	char* str;
} SnString;

struct SnLinkBuffer;

CAPI SnString* snow_create_string(const char* str);
CAPI SnString* snow_create_string_from_linkbuffer(struct SnLinkBuffer* buf);

#endif /* end of include guard: STRING_H_OYTL2E1P */
