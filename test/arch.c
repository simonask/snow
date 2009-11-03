#include "test/test.h"
#include "snow/arch.h"

TEST_CASE(bit_test) {
	byte x = 0xa5; // 10100101
	TEST(snow_bit_test(&x, 0));
	TEST(!snow_bit_test(&x, 1));
	TEST(snow_bit_test(&x, 2));
	TEST(!snow_bit_test(&x, 3));
	TEST(!snow_bit_test(&x, 4));
	TEST(snow_bit_test(&x, 5));
	TEST(!snow_bit_test(&x, 6));
	TEST(snow_bit_test(&x, 7));
}

TEST_CASE(bit_test_and_set) {
	byte x = 0xa5; // 10100101
	TEST(snow_bit_test_and_set(&x, 0));
	TEST(!snow_bit_test_and_set(&x, 1));
	TEST(snow_bit_test_and_set(&x, 2));
	TEST(!snow_bit_test_and_set(&x, 3));
	TEST(!snow_bit_test_and_set(&x, 4));
	TEST(snow_bit_test_and_set(&x, 5));
	TEST(!snow_bit_test_and_set(&x, 6));
	TEST(snow_bit_test_and_set(&x, 7));
	
	TEST(x == 0xff);
}

TEST_CASE(bit_test_and_clear) {
	byte x = 0xa5; // 10100101
	TEST(snow_bit_test_and_clear(&x, 0));
	TEST(!snow_bit_test_and_clear(&x, 1));
	TEST(snow_bit_test_and_clear(&x, 2));
	TEST(!snow_bit_test_and_clear(&x, 3));
	TEST(!snow_bit_test_and_clear(&x, 4));
	TEST(snow_bit_test_and_clear(&x, 5));
	TEST(!snow_bit_test_and_clear(&x, 6));
	TEST(snow_bit_test_and_clear(&x, 7));
	
	TEST(x == 0x0);
}

TEST_CASE(safe_increment) {
	byte b = 10;
	SAFE_INCR_BYTE(b);
	TEST(b == 11);
	b = 0xff;
	SAFE_INCR_BYTE(b);
	TEST(b == 0);
	
	uintx w = 0;
	SAFE_INCR_WORD(w);
	TEST(w == 1);
	w = UINTX_MAX;
	SAFE_INCR_WORD(w);
	TEST(w == 0);
}

TEST_CASE(safe_decrement) {
	byte b = 0xff;
	SAFE_DECR_BYTE(b);
	TEST(b == 0xfe);
	b = 0;
	SAFE_DECR_BYTE(b);
	TEST(b == 0xff);
	
	uintx w = UINTX_MAX;
	SAFE_DECR_WORD(w);
	TEST(w == UINTX_MAX-1);
	w = 0;
	SAFE_DECR_WORD(w);
	TEST(w == UINTX_MAX);
}

