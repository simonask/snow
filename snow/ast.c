#include "snow/ast.h"
#include "snow/array.h"
#include "snow/intern.h"
#include "snow/str.h"
#include <stdarg.h>

// Order is crucial!
static uintx ast_size[] = {
	1, //SN_AST_LITERAL,
	1, //SN_AST_SEQUENCE,
	4, //SN_AST_FUNCTION,
	1, //SN_AST_RETURN,
	0, //SN_AST_BREAK,
	0, //SN_AST_CONTINUE,
	0, //SN_AST_SELF,
	0, //SN_AST_CURRENT_SCOPE,
	1, //SN_AST_LOCAL,
	3, //SN_AST_MEMBER,
	2, //SN_AST_LOCAL_ASSIGNMENT,
	3, //SN_AST_MEMBER_ASSIGNMENT,
	3, //SN_AST_IF_ELSE,
	2, //SN_AST_CALL,
	2, //SN_AST_LOOP,
	1, //SN_AST_TRY,
	2, //SN_AST_AND,
	2, //SN_AST_OR,
	2, //SN_AST_XOR,
	1, //SN_AST_NOT,
	1, //SN_AST_PARALLEL_THREAD,
	1, //SN_AST_PARALLEL_FORK
};

static SnAstNode* create_ast_node(SnAstNodeType type, ...)
{
	SnAstNode* node = (SnAstNode*)snow_alloc_any_object(SN_AST_TYPE, sizeof(SnAstNode) + ast_size[type]*sizeof(VALUE));
	node->type = type;
	node->size = ast_size[type];
	
	va_list ap;
	va_start(ap, type);
	uintx i;
	for (i = 0; i < node->size; ++i) {
		node->children[i] = va_arg(ap, VALUE);
	}
	va_end(ap);
	
	return node;
}

SnAstNode* snow_ast_literal(VALUE val) {
	return create_ast_node(SN_AST_LITERAL, val);
}

SnAstNode* snow_ast_sequence(uintx num, ...) {
	SnAstNode* seq = create_ast_node(SN_AST_SEQUENCE, snow_create_array_with_size(num));
	SnArray* ar = (SnArray*)seq->children[0];
	va_list ap;
	va_start(ap, num);
	for (uintx i = 0; i < num; ++i) {
		snow_array_set(ar, i, va_arg(ap, VALUE));
	}
	va_end(ap);
	return seq;
}

void snow_ast_sequence_push(SnAstNode* seq, VALUE val) {
	ASSERT(seq->type == SN_AST_SEQUENCE);
	ASSERT(seq->children[0]);
	ASSERT_TYPE(seq->children[0], SN_ARRAY_TYPE);
	snow_array_push((SnArray*)seq->children[0], val);
}

SnAstNode* snow_ast_function(const char* name, const char* file, SnAstNode* seq_args, SnAstNode* seq_def) {
	ASSERT(!seq_args || (seq_args->type == SN_AST_SEQUENCE));
	ASSERT(seq_def->type == SN_AST_SEQUENCE);
	return create_ast_node(SN_AST_FUNCTION, snow_create_string(name), snow_create_string(file), seq_args, seq_def);
}

SnAstNode* snow_ast_return(SnAstNode* node) {
	return create_ast_node(SN_AST_RETURN, node);
}

SnAstNode* snow_ast_break() { return create_ast_node(SN_AST_BREAK); }
SnAstNode* snow_ast_continue() { return create_ast_node(SN_AST_CONTINUE); }
SnAstNode* snow_ast_self() { return create_ast_node(SN_AST_SELF); }
SnAstNode* snow_ast_current_scope() { return create_ast_node(SN_AST_CURRENT_SCOPE); }
SnAstNode* snow_ast_local(SnSymbol sym) { return create_ast_node(SN_AST_LOCAL, symbol_to_value(sym)); }
SnAstNode* snow_ast_member(SnAstNode* self, SnSymbol member) { return create_ast_node(SN_AST_MEMBER, self, symbol_to_value(member)); }
SnAstNode* snow_ast_local_assign(SnSymbol sym, SnAstNode* node) { return create_ast_node(SN_AST_LOCAL_ASSIGNMENT, symbol_to_value(sym), node); }
SnAstNode* snow_ast_member_assign(SnAstNode* member, SnAstNode* node) {
	ASSERT(member->type == SN_AST_MEMBER);
	return create_ast_node(SN_AST_MEMBER_ASSIGNMENT, member, node);
}
SnAstNode* snow_ast_if_else(SnAstNode* expr, SnAstNode* body, SnAstNode* else_body) { return create_ast_node(SN_AST_IF_ELSE, expr, body, else_body); }
SnAstNode* snow_ast_call(SnAstNode* func, SnAstNode* seq_args) {
	ASSERT(!seq_args || (seq_args->type == SN_AST_SEQUENCE));
	return create_ast_node(SN_AST_CALL, func, seq_args);
}
SnAstNode* snow_ast_loop(SnAstNode* while_true, SnAstNode* body) { return create_ast_node(SN_AST_LOOP, while_true, body); }
SnAstNode* snow_ast_try(SnAstNode* body) { return create_ast_node(SN_AST_TRY, body); }
SnAstNode* snow_ast_and(SnAstNode* left, SnAstNode* right) { return create_ast_node(SN_AST_AND, left, right); }
SnAstNode* snow_ast_or(SnAstNode* left, SnAstNode* right)  { return create_ast_node(SN_AST_OR, left, right); }
SnAstNode* snow_ast_xor(SnAstNode* left, SnAstNode* right) { return create_ast_node(SN_AST_XOR, left, right); }
SnAstNode* snow_ast_not(SnAstNode* expr) { return create_ast_node(SN_AST_NOT, expr); }
SnAstNode* snow_ast_parallel_thread(SnAstNode* seq) { return create_ast_node(SN_AST_PARALLEL_THREAD, seq); }
SnAstNode* snow_ast_parallel_fork(SnAstNode* seq) { return create_ast_node(SN_AST_PARALLEL_FORK, seq); }


void init_ast_class(SnClass* klass)
{
	
}
