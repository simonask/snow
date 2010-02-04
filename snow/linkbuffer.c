#include "snow/linkbuffer.h"
#include "snow/intern.h"
#include <stdlib.h>
#include <string.h>


typedef struct SnLinkBufferPage {
	struct SnLinkBufferPage* next;
	uintx offset;
	byte data[];
} SnLinkBufferPage;


SnLinkBuffer* snow_create_linkbuffer(uintx page_size) {
	SnLinkBuffer* buf = (SnLinkBuffer*)snow_gc_alloc_atomic(sizeof(SnLinkBuffer));
	snow_gc_set_free_func(buf, (SnGCFreeFunc)snow_linkbuffer_clear);
	snow_init_linkbuffer(buf, page_size);
	return buf;
}

void snow_init_linkbuffer(SnLinkBuffer* buf, uintx page_size) {
	buf->page_size = page_size;
	buf->head = NULL;
	buf->tail = NULL;
}

void snow_free_linkbuffer(SnLinkBuffer* buf) {
	snow_linkbuffer_clear(buf);
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
		buf->tail = (SnLinkBufferPage*)snow_malloc(sizeof(SnLinkBufferPage) + buf->page_size);
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

uintx snow_linkbuffer_push_string(SnLinkBuffer* buf, const char* c) {
	uintx n = 0;
	while (*c) {
		n += snow_linkbuffer_push(buf, *c++);
	}
	return n;
}

uintx snow_linkbuffer_push_data(SnLinkBuffer* buf, const byte* data, uintx len) {
	uintx n = 0;
	for (uintx i = 0; i < len; ++i) {
		n += snow_linkbuffer_push(buf, data[i]);
	}
	return n;
}

uintx snow_linkbuffer_copy_data(SnLinkBuffer* buf, void* _dst, uintx n) {
	SnLinkBufferPage* current = buf->head;
	uintx copied = 0;
	byte* dst = (byte*)_dst;
	while (current && copied < n) {
		uintx remaining = n - copied; // remaining space in dst
		uintx to_copy = remaining < current->offset ? remaining : current->offset;
		memcpy(&dst[copied], current->data, to_copy);
		copied += to_copy;
		current = current->next;
	}
	return copied;
}

uintx snow_linkbuffer_modify(SnLinkBuffer* buf, uintx offset, uintx len, byte* new_data) {
	uintx buffer_offset = 0;
	uintx copied = 0;
	SnLinkBufferPage* current = buf->head;
	while (current && copied < len) {
		uintx next_offset = offset + copied;
		if (next_offset >= buffer_offset && next_offset < buffer_offset + current->offset)
		{
			uintx page_offset = next_offset - buffer_offset;
			uintx remaining = current->offset - page_offset; // remaining space in current page
			uintx to_copy = remaining < len ? remaining : len;
			memcpy(&current->data[page_offset], &new_data[copied], to_copy);
			copied += to_copy;
		}
		
		buffer_offset += current->offset;
		current = current->next;
	}
	return copied;
}

void snow_linkbuffer_clear(SnLinkBuffer* buf) {
	SnLinkBufferPage* page = buf->head;
	while (page) {
		SnLinkBufferPage* next = page->next;
		snow_free(page);
		page = next;
	}
	buf->head = NULL;
	buf->tail = NULL;
}