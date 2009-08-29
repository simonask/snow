#include "snow/fixed-alloc.h"
#include "snow/intern.h"

void* fixed_alloc(struct fixed_alloc_t* alloc)
{
	void* ptr = alloc->free_element;
	if (ptr)
		alloc->free_element = *((void**)alloc->free_element);
	else
		ptr = malloc(alloc->page_size);
	
	#ifdef DEBUG
	memset(ptr, 0xcd, alloc->page_size);
	#endif
	
	return ptr;
}

void fixed_free(struct fixed_alloc_t* alloc, void* ptr)
{
	if (ptr)
	{
		#ifdef DEBUG
		memset(ptr, 0xef, alloc->page_size);
		#endif
		
		*((void**)ptr) = alloc->free_element;
		alloc->free_element = ptr;
	}
}