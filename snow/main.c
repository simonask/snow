#include "snow/snow.h"
#include "snow/object.h"
#include "snow/gc.h"
#include "snow/intern.h"
#include "snow/function.h"
#include "snow/codegen.h"
#include <stdio.h>

VALUE print_object(SnContinuation* me)
{
	printf("printing... 0x%llx\n", me->self);
}

static void interactive_prompt()
{
	SnObject* obj = snow_create_object(NULL);
	SnObject* obj2 = snow_create_object(obj);
	
	snow_object_set_member(obj, snow_symbol("hej"), obj2);
	snow_object_set_member(obj, snow_symbol("foo"), int_to_value(123));
	snow_object_set_member(obj, snow_symbol("p"), snow_create_function(print_object));
	snow_call_method(obj, snow_symbol("p"), 0);
	
	VALUE a = symbol_to_value(snow_symbol("a"));
	VALUE b = symbol_to_value(snow_symbol("b"));
	VALUE plus = symbol_to_value(snow_symbol("+"));
	SnAstNode* root = snow_ast_function("<no name>", "<no file>", snow_ast_sequence(2, a, b),
			snow_ast_sequence(2,
				snow_ast_call(snow_ast_member(snow_ast_local(snow_symbol("a")), snow_symbol("+")), snow_ast_sequence(1, snow_ast_local(snow_symbol("b"))))
			)
		);
	
	SnCodegen* cg = snow_create_codegen(root);

	SnFunction* func = snow_codegen_compile(cg);
//	VALUE val = snow_call(SN_NIL, func, 0);
//	debug("result: %s\n", value_to_string(val));
}

int main(int argc, char const *argv[])
{
	snow_init();
	interactive_prompt();
	return 0;
}