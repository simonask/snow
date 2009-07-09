#ifndef TEST_H_QQIZZJUC
#define TEST_H_QQIZZJUC

#define TEST_MAIN void _test_main()

#define TEST(x) if (!(x)) { FAIL(#x); }

#define FAIL(msg) _test_fail(msg, __FILE__, __LINE__)
#define PENDING() _test_pending()

void _test_main();
void _test_fail(const char*, const char* file, int line);
void _test_pending();

#endif /* end of include guard: TEST_H_QQIZZJUC */
