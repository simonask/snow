#include "snow/wrapper.h"

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

SnObject* create_wrapper_prototype()
{
	SnObject* proto = snow_create_object(NULL);
	return proto;
}