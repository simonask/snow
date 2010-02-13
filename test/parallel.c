#include "test/test.h"
#include "snow/task.h"
#include "snow/context.h"
#include "snow/function.h"
#include "snow/intern.h"
#include "snow/lock-impl.h"

static void func_for_each(void* data, size_t element_size, size_t i, void* userdata) {
	intx* nx = (intx*)data;
	++nx[i];
}

TEST_CASE(parallel_for_each) {
	intx nx[] = {0, 0, 0, 0, 0, 0, 0, 0};
	snow_parallel_for_each(nx, sizeof(intx), 8, func_for_each, NULL);
	for (size_t i = 0; i < 8; ++i) {
		TEST(nx[i] == 1);
	}
}

/*
static intx lock_test = 0;
static SnLock lock;

static void lock_test_func(size_t i, void* userdata) {
	snow_lock(&lock);
	++lock_test;
	snow_unlock(&lock);
}

TEST_CASE(lock) {
	snow_init_lock(&lock);
	SnParallelCallback funcs[] = {lock_test_func, lock_test_func, lock_test_func, lock_test_func, lock_test_func, lock_test_func, lock_test_func, lock_test_func};
	snow_parallel_call_each(funcs, 8, NULL);
	TEST(lock_test == 8);
}

static intx rlock_test = 0;
static SnRecursiveLock rlock;

static void rlock_incr(size_t n) {
	if (!n)
		return;
	snow_recursive_lock(&rlock);
	++rlock_test;
	rlock_incr(n-1);
	snow_recursive_unlock(&rlock);
}

static void rlock_test_func(size_t i, void* userdata) {
	rlock_incr(8);
}

TEST_CASE(recursive_lock) {
	snow_init_recursive_lock(&rlock);
	SnParallelCallback funcs[] = { rlock_test_func, rlock_test_func };
	snow_parallel_call_each(funcs, 2, NULL);
	TEST(rlock_test == 16);
}
*/
