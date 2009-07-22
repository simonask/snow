#include "snow/snow.h"
#include "snow/object.h"
#include "snow/gc.h"
#include "snow/intern.h"
#include "snow/function.h"
#include "snow/codegen.h"
#include <stdio.h>

VALUE print_object(SnContext* me)
{
	printf("printing... 0x%llx\n", me->self);
	return SN_NIL;
}

static void interactive_prompt()
{
	SnObject* obj = snow_create_object(NULL);
	SnObject* obj2 = snow_create_object(obj);
	
	snow_set_member(obj, snow_symbol("hej"), obj2);
	snow_set_member(obj, snow_symbol("foo"), int_to_value(123));
	snow_set_member(obj, snow_symbol("p"), snow_create_function(print_object));
	snow_call_method(obj, snow_symbol("p"), 0);
}

int main(int argc, char const *argv[])
{
	snow_init();
	interactive_prompt();
	return 0;
}