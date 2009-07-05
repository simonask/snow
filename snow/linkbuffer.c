#include "snow/linkbuffer.h"
#include <stdlib.h>

SnLinkBuffer* snow_create_linkbuffer(uintx page_size) {
	SnLinkBuffer* buf = (SnLinkBuffer*)malloc(sizeof(SnLinkBuffer));
	buf->page_size = page_size;
	buf->head = NULL;
	buf->tail = NULL;
	return buf;
}

void snow_free_linkbuffer(SnLinkBuffer* buf) {
	SnLinkBufferPage* page = buf->head;
	while (page) {
		free(page);
		page = page->next;
	}
	free(buf);
}

uintx snow_linkbuffer_size(SnLinkBuffer* buf) {
	uintx size = 0;
	SnLinkBufferPage* page = buf->head;
	while (page) {
		if (page->next)
			size += buf->page_size;
		else
			size += page->offset;
		page = page->next;
	}
	return size;
}

uintx snow_linkbuffer_push(SnLinkBuffer* buf, byte b) {
	if (!buf->tail || buf->tail->offset == buf->page_size) {
		SnLinkBufferPage* old_tail = buf->tail;
		buf->tail = (SnLinkBufferPage*)malloc(sizeof(SnLinkBufferPage) + buf->page_size);
		buf->tail->next = NULL;
		buf->tail->offset = 0;
		if (!buf->head)
			buf->head = buf->tail;
		if (old_tail)
			old_tail->next = buf->tail;
	}
	
	buf->tail->data[buf->tail->offset++] = b;
	return 1;
}

uintx snow_linkbuffer_copy_data(SnLinkBuffer*, void* dst, uintx n);
uintx snow_linkbuffer_modify(SnLinkBuffer*, uintx offset, uintx len, byte* new_data);