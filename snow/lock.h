#ifndef LOCK_H_6MWOAQD4
#define LOCK_H_6MWOAQD4

#include "snow/basic.h"

typedef struct PACKED {
	uintx locked_by_task_id;
	byte locked;
} SnLock;

typedef struct PACKED {
	uintx locked_by_task_id;
	byte locked;
	byte recursive_count;
} SnRecursiveLock;

static inline void snow_init_lock(SnLock* lock);
static inline bool snow_is_locked(SnLock* lock);
static inline bool snow_try_lock(SnLock* lock);
static inline void snow_lock(SnLock* lock);
static inline void snow_unlock(SnLock* lock);
static inline void snow_init_recursive_lock(SnRecursiveLock* lock);
static inline bool snow_try_recursive_lock(SnRecursiveLock* lock);
static inline void snow_recursive_lock(SnRecursiveLock* lock);
static inline void snow_recursive_unlock(SnRecursiveLock* lock);


#endif /* end of include guard: LOCK_H_6MWOAQD4 */
