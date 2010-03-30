#include "snow/ast.h"
#include "snow/array.h"
#include "snow/intern.h"
#include "snow/str.h"
#include "snow/linkbuffer.h"
#include <stdarg.h>
#include <stdio.h>

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
	2, //SN_AST_MEMBER,
	2, //SN_AST_LOCAL_ASSIGNMENT,
	3, //SN_AST_MEMBER_ASSIGNMENT,
	3, //SN_AST_IF_ELSE,
	2, //SN_AST_CALL,
	2, //SN_AST_LOOP,
	3, //SN_AST_TRY,
	3, //SN_AST_CATCH,
	2, //SN_AST_AND,
	2, //SN_AST_OR,
	2, //SN_AST_XOR,
	1, //SN_AST_NOT,
	1, //SN_AST_PARALLEL_THREAD,
	1, //SN_AST_PARALLEL_FORK
};

static const char* ast_name[] = {
	"Literal",
	"Sequence",
	"Function",
	"Return",
	"Break",
	"Continue",
	"Self",
	"Here",
	"Local",
	"Member",
	"LocalAssignment",
	"MemberAssignment",
	"IfElse",
	"Call",
	"Loop",
	"Try",
	"Catch",
	"And",
	"Or",
	"Xor",
	"Not",
	"ParallelThread",
	"ParallelFork"
};

static SnAstNode* create_ast_node(SnAstNodeTypes type, ...)
{
	SnAstNode* node = SNOW_GC_ALLOC_OBJECT(SnAstNode);
	node->node_type = type;
	node->child0 = node->child1 = node->child2 = node->child3 = NULL;
	
	size_t size = ast_size[type];
	va_list ap;
	va_start(ap, type);
	if (size >= 1)
		node->child0 = va_arg(ap, VALUE);
	if (size >= 2)
		node->child1 = va_arg(ap, VALUE);
	if (size >= 3)
		node->child2 = va_arg(ap, VALUE);
	if (size >= 4)
		node->child3 = va_arg(ap, VALUE);
	va_end(ap);
	
	return node;
}

void SnAstNode_finalize(SnAstNode* node) {
	// nothing
}

SnAstNode* snow_ast_literal(VALUE val) {
	return create_ast_node(SN_AST_LITERAL, val);
}

SnAstNode* snow_ast_sequence(uintx num, ...) {
	SnAstNode* seq = create_ast_node(SN_AST_SEQUENCE, snow_create_array_with_size(num));
	SnArray* ar = (SnArray*)seq->child0;
	va_list ap;
	va_start(ap, num);
	for (uintx i = 0; i < num; ++i) {
		snow_array_set(ar, i, va_arg(ap, VALUE));
	}
	va_end(ap);
	return seq;
}

void snow_ast_sequence_push(SnAstNode* seq, VALUE val) {
	ASSERT(seq->node_type == SN_AST_SEQUENCE);
	ASSERT(seq->child0);
	ASSERT_TYPE(seq->child0, SnArrayType);
	snow_array_push((SnArray*)seq->child0, val);
}

SnAstNode* snow_ast_function(const char* name, const char* file, SnAstNode* seq_args, SnAstNode* seq_def) {
	ASSERT(!seq_args || (seq_args->node_type == SN_AST_SEQUENCE));
	ASSERT(seq_def->node_type == SN_AST_SEQUENCE);
	return create_ast_node(SN_AST_FUNCTION, snow_create_string(name), snow_create_string(file), seq_args, seq_def);
}

SnAstNode* snow_ast_return(SnAstNode* node) {
	return create_ast_node(SN_AST_RETURN, node);
}

SnAstNode* snow_ast_break() { return create_ast_node(SN_AST_BREAK); }
SnAstNode* snow_ast_continue() { return create_ast_node(SN_AST_CONTINUE); }
SnAstNode* snow_ast_self() { return create_ast_node(SN_AST_SELF); }
SnAstNode* snow_ast_current_scope() { return create_ast_node(SN_AST_CURRENT_SCOPE); }
SnAstNode* snow_ast_local(SnSymbol sym) { return create_ast_node(SN_AST_LOCAL, snow_symbol_to_value(sym)); }
SnAstNode* snow_ast_member(SnAstNode* self, SnSymbol member) { return create_ast_node(SN_AST_MEMBER, self, snow_symbol_to_value(member)); }
SnAstNode* snow_ast_local_assign(SnSymbol sym, SnAstNode* node) { return create_ast_node(SN_AST_LOCAL_ASSIGNMENT, snow_symbol_to_value(sym), node); }
SnAstNode* snow_ast_member_assign(SnAstNode* member, SnAstNode* node) {
	ASSERT(member->node_type == SN_AST_MEMBER);
	return create_ast_node(SN_AST_MEMBER_ASSIGNMENT, member, node);
}
SnAstNode* snow_ast_if_else(SnAstNode* expr, SnAstNode* body, SnAstNode* else_body) { return create_ast_node(SN_AST_IF_ELSE, expr, body, else_body); }
SnAstNode* snow_ast_call(SnAstNode* func, SnAstNode* seq_args) {
	ASSERT(!seq_args || (seq_args->node_type == SN_AST_SEQUENCE));
	return create_ast_node(SN_AST_CALL, func, seq_args);
}
SnAstNode* snow_ast_loop(SnAstNode* while_true, SnAstNode* body) { return create_ast_node(SN_AST_LOOP, while_true, body); }
SnAstNode* snow_ast_try(SnAstNode* body, SnAstNode* catch, SnAstNode* ensure) { return create_ast_node(SN_AST_TRY, body, catch, ensure); }
SnAstNode* snow_ast_catch(VALUE parameter, SnAstNode* condition, SnAstNode* body) { return create_ast_node(SN_AST_CATCH, parameter, condition, body); }
SnAstNode* snow_ast_and(SnAstNode* left, SnAstNode* right) { return create_ast_node(SN_AST_AND, left, right); }
SnAstNode* snow_ast_or(SnAstNode* left, SnAstNode* right)  { return create_ast_node(SN_AST_OR, left, right); }
SnAstNode* snow_ast_xor(SnAstNode* left, SnAstNode* right) { return create_ast_node(SN_AST_XOR, left, right); }
SnAstNode* snow_ast_not(SnAstNode* expr) { return create_ast_node(SN_AST_NOT, expr); }
SnAstNode* snow_ast_parallel_thread(SnAstNode* seq) { return create_ast_node(SN_AST_PARALLEL_THREAD, seq); }
SnAstNode* snow_ast_parallel_fork(SnAstNode* seq) { return create_ast_node(SN_AST_PARALLEL_FORK, seq); }

SnSymbol snow_ast_type_name(SnAstNodeTypes type) {
	return snow_symbol(ast_name[type]);
}

VALUE snow_ast_node_get_child(SnAstNode* node, size_t child) {
	ASSERT(child < 4);
	return (&node->child0)[child];
}

void snow_ast_node_set_child(SnAstNode* node, size_t child, VALUE val) {
	ASSERT(child < 4);
	(&node->child0)[child] = val;
}

SNOW_FUNC(_ast_type) {
	ASSERT_TYPE(SELF, SnAstNodeType);
	SnAstNode* self = (SnAstNode*)SELF;
	return snow_symbol_to_value(snow_ast_type_name(self->node_type));
}

static void ast_pretty_print(VALUE root, intx indent) {
	char indentation[(indent*4)+1];
	memset(indentation, 0x20, sizeof(indentation));
	indentation[indent*4] = '\0';
	
	printf("%s", indentation);
	
	if (snow_typeof(root) == SnAstNodeType) {
		SnAstNode* node = (SnAstNode*)root;
		printf("<%s ", snow_symbol_to_cstr(snow_ast_type_name(node->node_type)));
		switch (node->node_type) {
			case SN_AST_LITERAL:
			case SN_AST_LOCAL:
			{
				printf("%s>\n", snow_inspect_value(node->child0));
				break;
			}
			default:
			{
				printf("\n");
				VALUE* p = &node->child0;
				for (uintx i = 0; i < ast_size[node->node_type]; ++i) {
					ast_pretty_print(p[i], indent+1);
				}
				printf("%s>\n", indentation);
				break;
			}
		}
	} else {
		if (snow_typeof(root) == SnArrayType) {
			SnArray* array = (SnArray*)root;
			printf("@(\n");
			for (size_t i = 0; i < snow_array_size(array); ++i) {
				ast_pretty_print(snow_array_get(array, i), indent+1);
				if (i != snow_array_size(array)-1)
					printf("%s,\n", indentation);
			}
			printf("%s)\n", indentation);
		} else {
			printf("%s\n", snow_inspect_value(root));
		}
	}
}

SNOW_FUNC(_ast_print) {
	ASSERT_TYPE(SELF, SnAstNodeType);
	SnAstNode* node = (SnAstNode*)SELF;
	ast_pretty_print(node, 0);
	return SN_NIL;
}

SNOW_FUNC(_ast_inspect) {
	ASSERT_TYPE(SELF, SnAstNodeType);	
	SnAstNode* node = (SnAstNode*)SELF;
	return snow_format_string("<AstNode(%@)>", snow_symbol_to_value(snow_ast_type_name(node->node_type)));
}

SNOW_FUNC(_ast_name) {
	ASSERT_TYPE(SELF, SnAstNodeType);
	SnAstNode* node = (SnAstNode*)SELF;
	return snow_symbol_to_value(snow_ast_type_name(node->node_type));
}

void SnAstNode_init_class(SnClass* klass)
{
	snow_define_property(klass, "__name__", _ast_name, NULL);
	snow_define_property(klass, "type", _ast_type, NULL);
	snow_define_method(klass, "inspect", _ast_inspect);
	snow_define_method(klass, "print", _ast_print);
}
