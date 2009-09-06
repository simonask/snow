#include "snow/fixed-alloc.h"
#include "snow/intern.h"
#include "snow/gc.h"
#include <string.h>
#include <stdlib.h>

void* fixed_alloc(struct fixed_alloc_t* alloc)
{
	if (!alloc->mutex_initialized)
	{
		pthread_mutex_init(&alloc->mutex, NULL);
		alloc->mutex_initialized = true;
	}
	
	pthread_mutex_lock(&alloc->mutex);
	void* ptr = alloc->free_element;
	if (ptr)
		alloc->free_element = *((void**)alloc->free_element);
	else
		ptr = snow_malloc(alloc->page_size);
	pthread_mutex_unlock(&alloc->mutex);
	
	#ifdef DEBUG
	memset(ptr, 0xcd, alloc->page_size);
	#endif
	
	return ptr;
}

void fixed_free(struct fixed_alloc_t* alloc, void* ptr)
{
	if (ptr)
	{
		ASSERT(alloc->mutex_initialized);
		
		#ifdef DEBUG
		memset(ptr, 0xef, alloc->page_size);
		#endif
		
		pthread_mutex_lock(&alloc->mutex);
		*((void**)ptr) = alloc->free_element;
		alloc->free_element = ptr;
		pthread_mutex_unlock(&alloc->mutex);
	}
}
