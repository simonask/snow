#ifndef FIXED_ALLOC_H_7VP6DMRX
#define FIXED_ALLOC_H_7VP6DMRX

#include "snow/basic.h"
#include <pthread.h>

struct fixed_alloc_t {
	// TODO: Mutex lock
	uintx page_size;
	void* free_element;
	pthread_mutex_t mutex;
	bool mutex_initialized;
};

#define FIXED_ALLOCATOR(NAME, SIZE) struct fixed_alloc_t NAME = {.page_size = SIZE, .free_element = NULL, .mutex_initialized = false}

HIDDEN void* fixed_alloc(struct fixed_alloc_t* allocator);
HIDDEN void fixed_free(struct fixed_alloc_t* allocator, void* ptr);

#endif /* end of include guard: FIXED_ALLOC_H_7VP6DMRX */
