#include "test/test.h"
#include "snow/intern.h"
#include "snow/snow.h"
#include <stdlib.h>
#include <setjmp.h>
#include <stdio.h>

#define RED "\x1b[1;31m"
#define YELLOW "\x1b[1;33m"
#define GREEN "\x1b[1;32m"
#define RESET_COLOR "\x1b[0m"

static jmp_buf fail_buf;

struct test_case {
	void(*func)();
	const char* name;
	struct test_case* next;
};

static struct test_case* first_case = NULL;
static struct test_case* last_case = NULL;
static int test = 0xcdefabcd;

int main (int argc, char const *argv[])
{
	snow_init();
	
	int ok = 0;
	int failed = 0;
	int pending = 0;
	
	struct test_case* test_case = first_case;
	while (test_case) {
		printf("%-60s", test_case->name);
		
		switch (setjmp(fail_buf))
		{
			case 0:
			  test_case->func(); ++ok;
			  printf(GREEN "ok" RESET_COLOR "\n");
			  break;
			case 1: ++failed; break;
			case 2: ++pending; break;
			default: TRAP();
		}
		
		test_case = test_case->next;
	}
	printf("%d test%s passed, %d failed, %d pending\n", ok, ok == 1 ? "" : "s", failed, pending);
	
	return failed;
}

void _test_fail(const char* msg, const char* file, int line) {
	printf(RED "failed" RESET_COLOR "\n");
	fprintf(stderr, "failed at %s:%d: %s\n", file, line, msg);
	longjmp(fail_buf, 1);
}

void _test_pending() {
	printf(YELLOW "pending" RESET_COLOR "\n");
	longjmp(fail_buf, 2);
}

void _register_test(const char* name, void(*func)()) {
	ASSERT(test == 0xcdefabcd); // make sure statics have been set!
	struct test_case* test_case = (struct test_case*)snow_malloc(sizeof(struct test_case));
	test_case->func = func;
	test_case->name = name;
	test_case->next = NULL;
	if (!first_case)
		first_case = test_case;
	if (last_case)
		last_case->next = test_case;
	last_case = test_case;
}