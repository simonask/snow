#ifndef WRAPPER_H_4DAR1ROT
#define WRAPPER_H_4DAR1ROT

#include "snow/object.h"

CAPI SnPointer* snow_create_pointer(void* ptr, SnPointerFreeFunc free_func);
static inline void* snow_pointer_get_pointer(SnPointer* wrapper) { return wrapper->ptr; }
static inline void snow_pointer_set_pointer(SnPointer* wrapper, void* ptr) { wrapper->ptr = ptr; }

#endif /* end of include guard: WRAPPER_H_4DAR1ROT */
