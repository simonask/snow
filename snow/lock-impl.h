#ifndef LOCK_IMPL_H_UNIUIKUD
#define LOCK_IMPL_H_UNIUIKUD

/*#include "snow/arch.h"
#include "snow/task.h"
#include "snow/lock.h"
#include <unistd.h>

static inline void snow_init_lock(SnLock* lock);
static inline bool snow_is_locked(SnLock* lock);
static inline bool snow_try_lock(SnLock* lock);
static inline void snow_lock(SnLock* lock);
static inline void snow_unlock(SnLock* lock);
static inline void snow_init_recursive_lock(SnRecursiveLock* lock);
static inline bool snow_try_recursive_lock(SnRecursiveLock* lock);
static inline void snow_recursive_lock(SnRecursiveLock* lock);
static inline void snow_recursive_unlock(SnRecursiveLock* lock);

static inline void snow_init_lock(SnLock* lock) {
	lock->locked_by_task_id = 0;
	lock->locked = 0;
}

static inline bool snow_is_locked(SnLock* lock) {
	return snow_bit_test(&lock->locked, 0);
}

static inline bool snow_try_lock(SnLock* lock) {
	if (!snow_bit_test_and_set(&lock->locked, 0)) {
		lock->locked_by_task_id = snow_get_current_task_id();
		return true;
	}
	return false;
}

static inline void snow_lock(SnLock* lock) {
	while (!snow_try_lock(lock)) { usleep(0); }
}

static inline void snow_unlock(SnLock* lock) {
	ASSERT(lock->locked_by_task_id == snow_get_current_task_id()); // unlocking lock from another task. very illegal!
	lock->locked_by_task_id = 0;
	if (!snow_bit_test_and_clear(&lock->locked, 0)) {
		TRAP(); // trying to unlock unlocked lock. most likely a lock inconsistency somewhere.
	}
}

static inline void snow_init_recursive_lock(SnRecursiveLock* lock) {
	lock->locked_by_task_id = 0;
	lock->locked = 0;
	lock->recursive_count = 0;
}

static inline bool snow_try_recursive_lock(SnRecursiveLock* lock) {
	bool already_locked = snow_bit_test_and_set(&lock->locked, 0);
	
	if (already_locked) {
		if (lock->locked_by_task_id == snow_get_current_task_id()) {
			ASSERT(lock->recursive_count < 0xff); // max lock recursion reached
			SAFE_INCR_BYTE(lock->recursive_count);
			return true;
		}
		return false;
	} else {
		ASSERT(lock->recursive_count == 0);
		lock->locked_by_task_id = snow_get_current_task_id();
		SAFE_INCR_BYTE(lock->recursive_count);
		return true;
	}
}

static inline void snow_recursive_lock(SnRecursiveLock* lock) {
	while (!snow_try_recursive_lock(lock)) { usleep(0); }
}

static inline void snow_recursive_unlock(SnRecursiveLock* lock) {
	ASSERT(lock->locked_by_task_id == snow_get_current_task_id());
	ASSERT(lock->recursive_count > 0); // not locked!
	SAFE_DECR_BYTE(lock->recursive_count);
	if (lock->recursive_count == 0) {
		bool was_locked = snow_bit_test_and_clear(&lock->locked, 0);
		ASSERT(was_locked); // not locked!
	}
}*/

#endif /* end of include guard: LOCK_H_UNIUIKUD */
