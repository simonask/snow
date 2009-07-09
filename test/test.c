#include "test/test.h"
#include <setjmp.h>
#include <stdio.h>

static jmp_buf fail_buf;

int main (int argc, char const *argv[])
{
	int r = setjmp(fail_buf);
	if (!r) {
		_test_main();
	} else {
		return r;
	}
	return 0;
}

void _test_fail(const char* msg, const char* file, int line) {
	fprintf(stderr, "Test failed at %s:%d: %s\n", file, line, msg);
	longjmp(fail_buf, 1);
}

void _test_pending() {
	longjmp(fail_buf, 2);
}