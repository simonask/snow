#include "snow/pointer.h"
#include "snow/intern.h"

static void pointer_free(VALUE val)
{
	ASSERT_TYPE(val, SN_POINTER_TYPE);
	SnPointer* pointer = (SnPointer*)val;
	pointer->free_func(pointer->ptr);
}

SnPointer* snow_create_pointer(void* ptr, SnPointerFreeFunc free_func)
{
	SnPointer* pointer = (SnPointer*)snow_alloc_any_object(SN_POINTER_TYPE, sizeof(SnPointer));
	snow_gc_set_free_func(pointer, pointer_free);
	pointer->ptr = ptr;
	pointer->free_func = free_func;
	return pointer;
}

void init_pointer_class(SnClass* klass)
{
	
}