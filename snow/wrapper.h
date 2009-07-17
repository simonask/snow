#ifndef WRAPPER_H_4DAR1ROT
#define WRAPPER_H_4DAR1ROT

#include "snow/object.h"

typedef struct SnWrapper {
	SnObjectBase base;
	void* ptr;
} SnWrapper;

CAPI SnWrapper* snow_create_wrapper(void* ptr);
CAPI void* snow_wrapper_get_pointer(SnWrapper*);

#endif /* end of include guard: WRAPPER_H_4DAR1ROT */
