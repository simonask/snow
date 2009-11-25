#include "snow/wrapper.h"
#include "snow/intern.h"

static void wrapper_free(VALUE val)
{
	ASSERT_TYPE(val, SN_WRAPPER_TYPE);
	SnWrapper* wrapper = (SnWrapper*)val;
	wrapper->free_func(wrapper->ptr);
}

SnWrapper* snow_create_wrapper(void* ptr, SnWrapperFreeFunc free_func)
{
	SnWrapper* wrapper = (SnWrapper*)snow_alloc_any_object(SN_WRAPPER_TYPE, sizeof(SnWrapper));
	snow_gc_set_free_func(wrapper, wrapper_free);
	wrapper->ptr = ptr;
	wrapper->free_func = free_func;
	return wrapper;
}

void init_wrapper_class(SnClass* klass)
{
	
}