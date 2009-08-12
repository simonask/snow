#ifndef FIXED_ALLOC_H_7VP6DMRX
#define FIXED_ALLOC_H_7VP6DMRX

#include "snow/basic.h"

struct fixed_alloc_t {
	// TODO: Mutex lock
	uintx page_size;
	void* free_element;
};

#define FIXED_ALLOCATOR(NAME, SIZE) struct fixed_alloc_t NAME = {.page_size = SIZE, .free_element = NULL}

HIDDEN void* fixed_alloc(struct fixed_alloc_t* allocator);
HIDDEN void fixed_free(struct fixed_alloc_t* allocator, void* ptr);

#endif /* end of include guard: FIXED_ALLOC_H_7VP6DMRX */
