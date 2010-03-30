#ifndef AST_H_48SOBLU9
#define AST_H_48SOBLU9

#include "snow/basic.h"
#include "snow/symbol.h"
struct SnAstNode;

CAPI struct SnAstNode* snow_ast_literal(VALUE val);
CAPI struct SnAstNode* snow_ast_sequence(uintx num, ...);
CAPI void snow_ast_sequence_push(struct SnAstNode* seq, VALUE val);
CAPI struct SnAstNode* snow_ast_function(const char* name, const char* file, struct SnAstNode* seq_args, struct SnAstNode* seq_def);
CAPI struct SnAstNode* snow_ast_return(struct SnAstNode* node);
CAPI struct SnAstNode* snow_ast_break();
CAPI struct SnAstNode* snow_ast_continue();
CAPI struct SnAstNode* snow_ast_self();
CAPI struct SnAstNode* snow_ast_current_scope();
CAPI struct SnAstNode* snow_ast_local(SnSymbol sym);
CAPI struct SnAstNode* snow_ast_member(struct SnAstNode* self, SnSymbol member);
CAPI struct SnAstNode* snow_ast_local_assign(SnSymbol sym, struct SnAstNode* node);
CAPI struct SnAstNode* snow_ast_member_assign(struct SnAstNode* member, struct SnAstNode* node);
CAPI struct SnAstNode* snow_ast_if_else(struct SnAstNode* expr, struct SnAstNode* body, struct SnAstNode* else_body);
CAPI struct SnAstNode* snow_ast_call(struct SnAstNode* func, struct SnAstNode* seq_args);
CAPI struct SnAstNode* snow_ast_loop(struct SnAstNode* while_true, struct SnAstNode* body);
CAPI struct SnAstNode* snow_ast_try(struct SnAstNode* body, struct SnAstNode* catch, struct SnAstNode* ensure);
CAPI struct SnAstNode* snow_ast_catch(VALUE parameter, struct SnAstNode* condition, struct SnAstNode* body);
CAPI struct SnAstNode* snow_ast_and(struct SnAstNode* left, struct SnAstNode* right);
CAPI struct SnAstNode* snow_ast_or(struct SnAstNode* left, struct SnAstNode* right);
CAPI struct SnAstNode* snow_ast_xor(struct SnAstNode* left, struct SnAstNode* right);
CAPI struct SnAstNode* snow_ast_not(struct SnAstNode* expr);
CAPI struct SnAstNode* snow_ast_parallel_thread(struct SnAstNode* seq);
CAPI struct SnAstNode* snow_ast_parallel_fork(struct SnAstNode* seq);

CAPI SnSymbol snow_ast_type_name(SnAstNodeTypes);
CAPI VALUE snow_ast_node_get_child(struct SnAstNode* node, size_t child);
CAPI void snow_ast_node_set_child(struct SnAstNode* node, size_t child, VALUE val);

#endif /* end of include guard: AST_H_48SOBLU9 */
