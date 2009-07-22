#ifndef TEST_H_QQIZZJUC
#define TEST_H_QQIZZJUC

#include "snow/basic.h"

#define TEST_CASE(NAME) \
	static inline void _snow_test_ ## NAME ## _func(); \
	__attribute__((constructor)) static inline void _snow_test_register_ ## NAME() { _register_test(#NAME, _snow_test_ ## NAME ## _func); } \
	void _snow_test_ ## NAME ## _func()


#define TEST(x) if (!(x)) { FAIL(#x); }

#define FAIL(msg) _test_fail(msg, __FILE__, __LINE__)
#define PENDING() _test_pending()

void _register_test(const char* name, void(*)());
void _test_main();
void _test_fail(const char*, const char* file, int line);
void _test_pending();

#endif /* end of include guard: TEST_H_QQIZZJUC */
