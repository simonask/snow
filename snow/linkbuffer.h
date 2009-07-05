#ifndef LINKBUFFER_H_U08VA9BE
#define LINKBUFFER_H_U08VA9BE

#include "snow/basic.h"

typedef struct SnLinkBufferPage {
	struct SnLinkBufferPage* next;
	uintx offset;
	byte data[];
} SnLinkBufferPage;

typedef struct SnLinkBuffer {
	uintx page_size;
	SnLinkBufferPage* head;
	SnLinkBufferPage* tail;
} SnLinkBuffer;

CAPI SnLinkBuffer* snow_create_linkbuffer(uintx page_size);
CAPI void snow_free_linkbuffer(SnLinkBuffer*);
CAPI uintx snow_linkbuffer_size(SnLinkBuffer*);
CAPI uintx snow_linkbuffer_push(SnLinkBuffer*, byte);
CAPI uintx snow_linkbuffer_push_string(SnLinkBuffer*, const char*);
CAPI uintx snow_linkbuffer_copy_data(SnLinkBuffer*, void* dst, uintx n);
CAPI uintx snow_linkbuffer_modify(SnLinkBuffer*, uintx offset, uintx len, byte* new_data);
CAPI void snow_linkbuffer_clear(SnLinkBuffer*);

#endif /* end of include guard: LINKBUFFER_H_U08VA9BE */
