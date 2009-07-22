#include "test/test.h"
#include "snow/codegen.h"
#include "snow/snow.h"
#include "snow/intern.h"

/*TEST_CASE(simple_add) {
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
}*/

TEST_CASE(simple_add) {
	SnAstNode* def = snow_ast_function("<no name>", "<no file>", snow_ast_sequence(0),
		snow_ast_sequence(3,
			snow_ast_local_assign(snow_symbol("a"), snow_ast_literal(int_to_value(123))),
			snow_ast_local_assign(snow_symbol("b"), snow_ast_literal(int_to_value(456))),
			snow_ast_call(snow_ast_member(snow_ast_local(snow_symbol("a")), snow_symbol("+")), snow_ast_sequence(1, snow_ast_local(snow_symbol("b"))))
		)
	);
	SnCodegen* cg = snow_create_codegen(def);
	SnFunction* func = snow_codegen_compile(cg);
	VALUE ret = snow_call(NULL, func, 0);
	
	TEST(value_to_int(ret) == 579);
}

/*
TEST_CASE(simple_closure) {
	HandleScope _;

	RefPtr<FunctionDefinition> def = _function(
		_local_assign(_ident("a"), _lit_int(123)),
		_local_assign(_ident("func"),
			_function(
				_local_assign(_ident("b"), _lit_int(456)),
				_member_call(_ident("b"), _ident("+"), _seq(_ident("a")))
			)
		),
		_local_call(_ident("func"))
	);
	
	Handle<Function> f = compile(def);
	//dump_disasm(std::cout, f);
	ValueHandle ret = snow::call(NULL, f);
	
	TEST_EQ(integer(ret), 579);
}

TEST_CASE(object_get) {
	HandleScope _;
	RefPtr<FunctionDefinition> def = _function(
		_member(_ident("obj"), _ident("member"))
	);
	def->arguments.push_back(_ident("obj"));
	
	Handle<Object> obj = gc_new<Object>();
	obj->set_raw("member", value(456));
	
	Handle<Function> f = compile(def);
	ValueHandle ret = snow::call(NULL, f, obj.value());
	
	TEST_EQ(integer(ret), 456LL);
}

TEST_CASE(object_set) {
	HandleScope _;
	RefPtr<FunctionDefinition> def = _function(
		_member_assign(_ident("obj"), _ident("member"), _ident("value"))
	);
	def->arguments.push_back(_ident("obj"));
	def->arguments.push_back(_ident("value"));
	
	Handle<Object> obj = gc_new<Object>();
	Handle<Function> f = compile(def);
	ValueHandle ret = snow::call(NULL, f, obj.value(), value(456));
	
	TEST_EQ(integer(obj->get_raw("member").value()), 456LL);
}

TEST_CASE(simple_loop) {
	HandleScope _;
	RefPtr<FunctionDefinition> def = _function(
		_local_assign(_ident("a"), _lit_int(0)),
		_loop(
			// condition:
			_member_call(_ident("a"), _ident("<"), _seq(_lit_int(1000))),
			// body:
			_local_assign(_ident("a"), _member_call(_ident("a"), _ident("+"), _seq(_lit_int(1))))
		),
		_ident("a")
	);
	
	Handle<Function> f = compile(def);
	ValueHandle ret = snow::call(NULL, f);
	
	TEST_EQ(integer(ret), 1000LL);
}

TEST_CASE(premature_return) {
	HandleScope _;
	RefPtr<FunctionDefinition> def = _function(
		_return(_lit_int(123)),
		_lit_int(456)
	);
	
	Handle<Function> f = compile(def);
	ValueHandle ret = snow::call(NULL, f);
	TEST_EQ(integer(ret), 123LL);
}

TEST_CASE(if_condition) {
	HandleScope _;
	RefPtr<FunctionDefinition> def = _function(
		_if(
			// condition:
			_member_call(_ident("a"), _ident("="), _seq(_lit_str("This is a test"))),
			// body:
			_return(_lit_int(1))
		),
		_lit_int(0)
	);
	def->arguments.push_back(_ident("a"));
	
	Handle<Function> f = compile(def);
	TEST_EQ(snow::call(NULL, f), value(0));
	TEST_EQ(snow::call(NULL, f, gc_new<String>("This is a test")), value(1));
	TEST_EQ(snow::call(NULL, f, value(123)), value(0));
}

TEST_CASE(unless_condition) {
	HandleScope _;
	RefPtr<FunctionDefinition> def = _function(
		_if(
			// condition:
			_member_call(_ident("a"), _ident("="), _seq(_lit_str("A test, this is"))),
			// body:
			_return(_lit_int(1)),
			// unless?:
			true
		),
		_lit_int(0)
	);
	def->arguments.push_back(_ident("a"));
	
	Handle<Function> f = compile(def);
	TEST_EQ(snow::call(NULL, f), value(1));
	TEST_EQ(snow::call(NULL, f, gc_new<String>("A test, this is")), value(0));
	TEST_EQ(snow::call(NULL, f, value(123)), value(1));
}

TEST_CASE(if_else_condition) {
	HandleScope _;
	// also tests the result of if-expressions
	RefPtr<FunctionDefinition> def = _function(
		_if_else(
			// condition:
			_member_call(_ident("a"), _ident("="), _seq(_lit_str("This is a test"))),
			// if true:
			_lit_int(123LL),
			// if false:
			_lit_int(456LL)
		)
	);
	def->arguments.push_back(_ident("a"));
	
	Handle<Function> f = compile(def);
	TEST_EQ(snow::call(NULL, f), value(456));
	TEST_EQ(snow::call(NULL, f, gc_new<String>("This is a test")), value(123));
	TEST_EQ(snow::call(NULL, f, gc_new<String>("LOL")), value(456));
}

TEST_CASE(unless_else_condition) {
	HandleScope _;
	// also tests the result of unless-expressions
	RefPtr<FunctionDefinition> def = _function(
		_if_else(
			// condition:
			_member_call(_ident("a"), _ident("="), _seq(_lit_str("A test, this is"))),
			// if true:
			_lit_int(999LL),
			// if false:
			_lit_int(771LL),
			// unless?:
			true
		)
	);
	def->arguments.push_back(_ident("a"));
	
	Handle<Function> f = compile(def);
	TEST_EQ(snow::call(NULL, f), value(999));
	TEST_EQ(snow::call(NULL, f, gc_new<String>("A test, this is")), value(771));
	TEST_EQ(snow::call(NULL, f, gc_new<String>("F00B4RZ!")), value(999));
}

TEST_CASE(logical_and) {
	HandleScope _;
	
	RefPtr<FunctionDefinition> def = _function(
		_logical_and(
			_ident("a"),
			_local_assign(_ident("b"), _lit_int(123LL))
		),
		_return(_ident("b"))
	);
	def->arguments.push_back(_ident("a"));
	
	Handle<Function> f = compile(def);
	TEST_EQ(snow::call(NULL, f, value(false)), nil());
	TEST_EQ(snow::call(NULL, f, nil()), nil());
	TEST_EQ(snow::call(NULL, f, value(0)), value(123));
	TEST_EQ(snow::call(NULL, f, value(456)), value(123));
}

TEST_CASE(logical_or) {
	HandleScope _;
	
	RefPtr<FunctionDefinition> def = _function(
		_logical_or(
			_ident("a"),
			_ident("b")
		)
	);
	def->arguments.push_back(_ident("a"));
	def->arguments.push_back(_ident("b"));
	
	Handle<Function> f = compile(def);
	TEST_EQ(snow::call(NULL, f, value(false), value(123)), value(123));
	TEST_EQ(snow::call(NULL, f, nil(), value(123)), value(123));
	TEST_EQ(snow::call(NULL, f, value(123), nil()), value(123));
	TEST_EQ(snow::call(NULL, f, nil(), value(false)), value(false));
}

TEST_CASE(logical_xor) {
	HandleScope _;
	RefPtr<FunctionDefinition> def = _function(
		_logical_xor(
			_ident("a"),
			_ident("b")
		)
	);
	def->arguments.push_back(_ident("a"));
	def->arguments.push_back(_ident("b"));
	
	Handle<Function> f = compile(def);
	TEST_EQ(snow::call(NULL, f, value(false), value(123)), value(123));
	TEST_EQ(snow::call(NULL, f, value(123), nil()), value(123));
	TEST_EQ(snow::call(NULL, f, nil(), nil()), value(false));
	TEST_EQ(snow::call(NULL, f, value(false), value(false)), value(false));
	TEST_EQ(snow::call(NULL, f, nil(), value(false)), value(false));
	TEST_EQ(snow::call(NULL, f, value(false), nil()), value(false));
}
*/