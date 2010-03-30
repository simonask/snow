#include "snow/pointer.h"
#include "snow/intern.h"

static void pointer_free(VALUE val)
{
	ASSERT_TYPE(val, SnPointerType);
	SnPointer* pointer = (SnPointer*)val;
	pointer->free_func(pointer->ptr);
}

SnPointer* snow_create_pointer(void* ptr, SnPointerFreeFunc free_func)
{
	SnPointer* pointer = SNOW_GC_ALLOC_OBJECT(SnPointer);
	pointer->ptr = ptr;
	pointer->free_func = free_func;
	return pointer;
}

void SnPointer_finalize(SnPointer* pointer) {
	if (pointer->free_func)
		pointer->free_func(pointer->ptr);
}

void SnPointer_init_class(SnClass* klass)
{
	
}