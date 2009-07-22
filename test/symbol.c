#include "test/test.h"
#include "snow/symbol.h"
#include <stdio.h>

TEST_CASE(identity) {
	SnSymbol sym1 = snow_symbol("foo");
	SnSymbol sym2 = snow_symbol("foo");
	TEST(sym1 == sym2);
	
	SnSymbol sym3 = snow_symbol("bar");
	TEST(sym1 != sym3);
}
