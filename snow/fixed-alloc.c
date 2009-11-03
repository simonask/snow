#include "snow/fixed-alloc.h"
#include "snow/intern.h"
#include "snow/gc.h"
#include <string.h>
#include <stdlib.h>
#include "snow/lock-impl.h"

void* fixed_alloc(struct fixed_alloc_t* alloc)
{
	if (!alloc->lock_initialized)
	{
		snow_init_lock(&alloc->lock);
		alloc->lock_initialized = true;
	}
	
	snow_lock(&alloc->lock);
	void* ptr = alloc->free_element;
	if (ptr)
		alloc->free_element = *((void**)alloc->free_element);
	else
		ptr = snow_malloc(alloc->page_size);
	snow_unlock(&alloc->lock);
	
	#ifdef DEBUG
	memset(ptr, 0xcd, alloc->page_size);
	#endif
	
	return ptr;
}

void fixed_free(struct fixed_alloc_t* alloc, void* ptr)
{
	if (ptr)
	{
		ASSERT(alloc->lock_initialized);
		
		#ifdef DEBUG
		memset(ptr, 0xef, alloc->page_size);
		#endif
		
		snow_lock(&alloc->lock);
		*((void**)ptr) = alloc->free_element;
		alloc->free_element = ptr;
		snow_unlock(&alloc->lock);
	}
}
