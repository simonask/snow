#ifndef WRAPPER_H_4DAR1ROT
#define WRAPPER_H_4DAR1ROT

#include "snow/object.h"

// TODO: Optimize this API to require less allocating

typedef void(*SnWrapperFreeFunc)(void* ptr);

typedef struct SnWrapper {
	SnObjectBase base;
	void* ptr;
	SnWrapperFreeFunc free_func;
} SnWrapper;

CAPI SnWrapper* snow_create_wrapper(void* ptr, SnWrapperFreeFunc free_func);
inline void* snow_wrapper_get_pointer(SnWrapper* wrapper) { return wrapper->ptr; }

#endif /* end of include guard: WRAPPER_H_4DAR1ROT */
