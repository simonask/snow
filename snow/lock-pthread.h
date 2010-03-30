#ifndef LOCK_PTHREAD_H_N9GZKPY1
#define LOCK_PTHREAD_H_N9GZKPY1

#include <pthread.h>
#include "snow/arch.h"

typedef struct SnLock {
	pthread_mutex_t m;
} SnLock;

static inline void snow_lock_init(SnLock* lock) { pthread_mutex_init(&lock->m, NULL); }
static inline void snow_lock_finalize(SnLock* lock) { int r = pthread_mutex_destroy(&lock->m); ASSERT(r == 0); }
static inline bool snow_try_lock(SnLock* lock) { return pthread_mutex_trylock(&lock->m) == 0; }
static inline void snow_lock(SnLock* lock) { pthread_mutex_lock(&lock->m); }
static inline void snow_unlock(SnLock* lock) { pthread_mutex_unlock(&lock->m); }

static inline SnLock* snow_lock_create() {
	SnLock* lock = (SnLock*)snow_malloc(sizeof(SnLock));
	snow_lock_init(lock);
	return lock;
}

static inline void snow_lock_destroy(SnLock* lock) {
	snow_lock_finalize(lock);
	snow_free(lock);
}

typedef struct SnRWLock {
	pthread_rwlock_t m;
} SnRWLock;

static inline void snow_rwlock_init(SnRWLock* lock) { pthread_rwlock_init(&lock->m, NULL); }
static inline void snow_rwlock_finalize(SnRWLock* lock) { int r = pthread_rwlock_destroy(&lock->m); ASSERT(r == 0); }
static inline bool snow_rwlock_try_read(SnRWLock* lock) { return pthread_rwlock_tryrdlock(&lock->m) == 0; }
static inline void snow_rwlock_read(SnRWLock* lock) { pthread_rwlock_rdlock(&lock->m); }
static inline bool snow_rwlock_try_write(SnRWLock* lock) { return pthread_rwlock_trywrlock(&lock->m) == 0; }
static inline void snow_rwlock_write(SnRWLock* lock) { pthread_rwlock_wrlock(&lock->m); }
static inline void snow_rwlock_unlock(SnRWLock* lock) { pthread_rwlock_unlock(&lock->m); }

static inline SnRWLock* snow_rwlock_create() {
	SnRWLock* lock = (SnRWLock*)snow_malloc(sizeof(SnRWLock));
	snow_rwlock_init(lock);
	return lock;
}

static inline void snow_rwlock_destroy(SnRWLock* lock) {
	snow_rwlock_finalize(lock);
	snow_free(lock);
}

#endif /* end of include guard: LOCK_PTHREAD_H_N9GZKPY1 */
