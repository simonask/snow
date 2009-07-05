#ifndef AST_H_48SOBLU9
#define AST_H_48SOBLU9

#include "snow/basic.h"
#include "snow/object.h"

typedef enum SnAstNodeType {
	SN_AST_LITERAL,
	SN_AST_SEQUENCE,
	SN_AST_FUNCTION,
	SN_AST_RETURN,
	SN_AST_BREAK,
	SN_AST_CONTINUE,
	SN_AST_SELF,
	SN_AST_LOCAL,
	SN_AST_MEMBER,
	SN_AST_LOCAL_ASSIGNMENT,
	SN_AST_MEMBER_ASSIGNMENT,
	SN_AST_IF_ELSE,
	SN_AST_CALL,
	SN_AST_LOOP,
	SN_AST_AND,
	SN_AST_OR,
	SN_AST_XOR,
	SN_AST_NOT,
} SnAstNodeType;

typedef struct SnAstNode {
	SnObjectBase base;
	SnAstNodeType type;
	uintx size;
	VALUE children[];
} SnAstNode;

CAPI SnAstNode* snow_ast_literal(VALUE val);
CAPI SnAstNode* snow_ast_sequence(uintx num, ...);
CAPI void snow_ast_sequence_push(SnAstNode* seq, VALUE val);
CAPI SnAstNode* snow_ast_function(const char* name, const char* file, SnAstNode* seq_args, SnAstNode* seq_def);
CAPI SnAstNode* snow_ast_return(SnAstNode* node);
CAPI SnAstNode* snow_ast_break();
CAPI SnAstNode* snow_ast_continue();
CAPI SnAstNode* snow_ast_self();
CAPI SnAstNode* snow_ast_local(SnSymbol sym);
CAPI SnAstNode* snow_ast_member(SnAstNode* self, SnSymbol member);
CAPI SnAstNode* snow_ast_local_assign(SnSymbol sym, SnAstNode* node);
CAPI SnAstNode* snow_ast_member_assign(SnAstNode* member, SnAstNode* node);
CAPI SnAstNode* snow_ast_if_else(SnAstNode* expr, SnAstNode* body, SnAstNode* else_body);
CAPI SnAstNode* snow_ast_call(SnAstNode* func, SnAstNode* seq_args);
CAPI SnAstNode* snow_ast_loop(SnAstNode* while_true, SnAstNode* body);
CAPI SnAstNode* snow_ast_and(SnAstNode* left, SnAstNode* right);
CAPI SnAstNode* snow_ast_or(SnAstNode* left, SnAstNode* right);
CAPI SnAstNode* snow_ast_xor(SnAstNode* left, SnAstNode* right);
CAPI SnAstNode* snow_ast_not(SnAstNode* expr);

#endif /* end of include guard: AST_H_48SOBLU9 */
