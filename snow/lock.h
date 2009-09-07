#ifndef LOCK_H_UNIUIKUD
#define LOCK_H_UNIUIKUD

#include "snow/basic.h"
#include "snow/arch.h"
#include <pthread.h>
#include <unistd.h>

static inline bool snow_try_lock(VALUE val) {
	if (!is_object(val))
		return true;
	
	SnObjectBase* obj = (SnObjectBase*)val;
	int32_t was_locked = 0;
	uint16_t me = (uint16_t)-1;
	
	#if defined(ARCH_x86_64) || defined(ARCH_x86_32)
	
	__asm__(
		"bts $0x1, %1\n"
		"jnc .not_locked\n"
		"movl $0x1, %0\n"
		".not_locked:\n"
		: "=m"(was_locked)
		: "m"(obj->flags)
	);
	
	#else
	
	// If your arch supports it, this is the operation that should be modeled atomically.
	// The following is very unsafe.
	was_locked = obj->flags & SN_OBJECT_FLAG_LOCKED;
	obj->flags |= SN_OBJECT_FLAG_LOCKED;
	
	#endif
	
	if (was_locked && obj->locked_by_thread == me)
		return true;
		
	if (!was_locked) {
		obj->locked_by_thread = me;
		return true;
	}
	
	return false;
}

static inline void snow_lock(VALUE val) {
	while (!snow_try_lock(val)) { usleep(0); }
}

static inline void snow_unlock(VALUE val) {
	if (!is_object(val)) return;
	SnObjectBase* obj = (SnObjectBase*)val;
	ASSERT(obj->locked_by_thread == (uint16_t)-1);
	obj->flags &= ~SN_OBJECT_FLAG_LOCKED;
}

#endif /* end of include guard: LOCK_H_UNIUIKUD */
