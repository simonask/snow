#include "test/test.h"
#include "snow/symbol.h"

TEST_MAIN {
	SnSymbol sym1 = snow_symbol("foo");
	SnSymbol sym2 = snow_symbol("foo");
	TEST(sym1 == sym2);
}