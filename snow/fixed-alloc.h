#ifndef FIXED_ALLOC_H_7VP6DMRX
#define FIXED_ALLOC_H_7VP6DMRX

#include "snow/basic.h"
#include "snow/lock.h"

struct fixed_alloc_t {
	uintx page_size;
	void* free_element;
	SnLock lock;
	bool lock_initialized;
};

#define FIXED_ALLOCATOR(NAME, SIZE) struct fixed_alloc_t NAME = {.page_size = SIZE, .free_element = NULL, .lock_initialized = false}

HIDDEN void* fixed_alloc(struct fixed_alloc_t* allocator);
HIDDEN void fixed_free(struct fixed_alloc_t* allocator, void* ptr);

#endif /* end of include guard: FIXED_ALLOC_H_7VP6DMRX */
