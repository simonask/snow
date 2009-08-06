#include "snow/wrapper.h"
#include "snow/intern.h"

SnWrapper* snow_create_wrapper(void* ptr)
{
	SnWrapper* wrapper = (SnWrapper*)snow_alloc_any_object(SN_WRAPPER_TYPE, sizeof(SnWrapper));
	wrapper->ptr = ptr;
	return wrapper;
}

void* snow_wrapper_get_pointer(SnWrapper* wrapper)
{
	return wrapper->ptr;
}

void init_wrapper_class(SnClass* klass)
{
	
}