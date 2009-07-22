#include "test/test.h"
#include "snow/codegen.h"
#include "snow/snow.h"
#include "snow/intern.h"

TEST_CASE(simple_add) {
	VALUE a = symbol_to_value(snow_symbol("a"));
	VALUE b = symbol_to_value(snow_symbol("b"));
	SnAstNode* root = snow_ast_function("<no name>", "<no file>", snow_ast_sequence(2, a, b),
			snow_ast_sequence(1,
				snow_ast_call(snow_ast_member(snow_ast_local(snow_symbol("a")), snow_symbol("+")), snow_ast_sequence(1, snow_ast_local(snow_symbol("b"))))
			)
		);
	SnCodegen* cg = snow_create_codegen(root);

	SnFunction* func = snow_codegen_compile(cg);
	VALUE val = snow_call(SN_NIL, func, 2, int_to_value(123), int_to_value(456));
	TEST(val == int_to_value(123+456));
}