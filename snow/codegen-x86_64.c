#include "snow/codegen.h"
#include "snow/codegen-intern.h"
#include "snow/snow.h"
#include "snow/intern.h"
#include "snow/asm-x86_64.h"
#include "snow/array.h"
#include "snow/gc.h"
#include "snow/context.h"
#include "snow/array.h"
#include "snow/arguments.h"
#include "snow/linkbuffer.h"

#include <stddef.h>

typedef struct SnCodegenX {
	SnCodegen base;
	SnLinkBuffer* returns;
	intx num_temporaries;
	SnArray* tmp_freelist;
} SnCodegenX;

static void codegen_compile_node(SnCodegenX* cgx, SnAstNode*);
static intx codegen_reserve_tmp(SnCodegenX* cgx);
static void codegen_free_tmp(SnCodegenX* cgx, intx tmp);
static bool codegen_is_local_from_parent(SnCodegenX* cgx, VALUE vsym);

SnCodegen* create_codegen(SnAstNode* root)
{
	ASSERT(root->type == SN_AST_FUNCTION && "Only function definitions can be used as root nodes in Snow ASTs.");
	SnCodegenX* codegen = (SnCodegenX*)snow_alloc_any_object(SN_CODEGEN_TYPE, sizeof(SnCodegenX));
	snow_gc_set_free_func(codegen, codegen_free);
	codegen->base.root = root;
	codegen->base.buffer = snow_create_linkbuffer(1024);
	codegen->returns = NULL;
	codegen->num_temporaries = 0;
	codegen->tmp_freelist = NULL;
	return (SnCodegen*)codegen;
}

void codegen_free(VALUE cg)
{
	SnCodegenX* cgx = (SnCodegenX*)cg;
	snow_free_linkbuffer(cgx->base.buffer);
}

intx codegen_reserve_tmp(SnCodegenX* cgx)
{
	if (cgx->tmp_freelist && snow_array_size(cgx->tmp_freelist) > 0)
		return value_to_int(snow_array_pop(cgx->tmp_freelist));
	else
		return cgx->num_temporaries++;
}

void codegen_free_tmp(SnCodegenX* cgx, intx tmp)
{
	if (!cgx->tmp_freelist)
		cgx->tmp_freelist = snow_create_array_with_size(32);
	snow_array_push(cgx->tmp_freelist, int_to_value(tmp));
}

#define ASM(instr, ...) asm_##instr(cgx->base.buffer, ##__VA_ARGS__)
#define RESERVE_TMP() codegen_reserve_tmp(cgx)
#define FREE_TMP(tmp) codegen_free_tmp(cgx, tmp)
#define TEMPORARY(tmp) ADDRESS(RBP, -(tmp+1) * sizeof(VALUE))
#define CALL(func) ASM(mov_id, IMMEDIATE(func), RCX); ASM(call, RCX); // TODO: auto-inlining

void codegen_compile_root(SnCodegen* cg)
{
	SnCodegenX* cgx = (SnCodegenX*)cg;
	ASSERT(cgx->base.root->type == SN_AST_FUNCTION);
	
	// prelude
	ASM(push, RBP);
	ASM(mov, RSP, RBP);
	int32_t stack_size_offset = ASM(sub_id, 0, RSP);
	ASM(push, R15);
	ASM(push, R14);
	ASM(mov, RDI, R15);                                           // function context in r15
	ASM(mov_rev, R14, ADDRESS(R15, offsetof(SnContext, locals))); // locals array in r14
	
	ASSERT(cgx->base.root->type == SN_AST_FUNCTION);
	
	// convert args to locals
	SnAstNode* args_seq = (SnAstNode*)cgx->base.root->children[2];
	if (args_seq)
	{
		ASSERT(args_seq->type == SN_AST_SEQUENCE);
		SnArray* args_array = (SnArray*)args_seq->children[0];
		ASSERT(args_array->base.base.type == SN_ARRAY_TYPE);
		cgx->base.result->argument_names = args_array;
		for (uintx i = 0; i < snow_array_size(args_array); ++i) {
			VALUE vsym = snow_array_get(args_array, i);
			ASSERT(is_symbol(vsym));

			intx idx = snow_function_description_add_local(cgx->base.result, value_to_symbol(vsym));

			ASM(mov, R15, RDI);
			ASM(mov_id, IMMEDIATE(vsym), RSI);
			CALL(snow_context_get_named_argument_by_value);
			ASM(mov, R14, RDI);
			ASM(mov_id, IMMEDIATE(idx), RSI);
			ASM(mov, RAX, RDX);
			CALL(snow_array_set);
		}
	} else {
		cgx->base.result->argument_names = snow_create_array();
	}
	
	// always clear rax before body, so empty functions will return nil.
	ASM(xor, RAX, RAX);
	
	// body
	SnAstNode* body_seq = (SnAstNode*)cgx->base.root->children[3];
	ASSERT(body_seq->type == SN_AST_SEQUENCE);
	codegen_compile_node(cgx, body_seq);
	
	// return
	Label return_label = ASM(label);
	ASM(bind, &return_label);
	ASM(pop, R14);
	ASM(pop, R15);
	ASM(leave);
	ASM(ret);
	
	// link all returns
	if (cgx->returns) {
		uintx bytes = snow_linkbuffer_size(cgx->returns);
		uintx len = bytes / sizeof(int32_t);
		int32_t offsets[bytes];
		snow_linkbuffer_copy_data(cgx->returns, offsets, bytes);
		
		for (uintx i = 0; i < len; ++i) {
			LabelRef ref;
			ref.offset = offsets[i];
			ref.label = &return_label;
			ASM(link, &ref);
		}
	}
	
	// link stack size
	uintx stack_size = cgx->num_temporaries * sizeof(VALUE);
	snow_linkbuffer_modify(cgx->base.buffer, stack_size_offset, 4, (byte*)&stack_size);
	
	
	if (cgx->tmp_freelist && snow_array_size(cgx->tmp_freelist) != cgx->num_temporaries)
	{
		ASSERT(false && "Unfreed temporaries! You likely have a temporary register leak, which will yield suboptimal stack space usage.");
	}
}

void codegen_compile_node(SnCodegenX* cgx, SnAstNode* node)
{
	ASSERT(node->base.type == SN_AST_TYPE);
	
	switch (node->type) {
		case SN_AST_LITERAL:
		{
			ASM(mov_id, IMMEDIATE(node->children[0]), RAX);
			break;
		}
		
		case SN_AST_SEQUENCE:
		{
			SnArray* seq = (SnArray*)node->children[0];
			ASSERT(seq->base.base.type == SN_ARRAY_TYPE);
			for (uintx i = 0; i < snow_array_size(seq); ++i) {
				codegen_compile_node(cgx, (SnAstNode*)snow_array_get(seq, i));
			}
			break;
		}
		
		case SN_AST_FUNCTION:
		{
			SnCodegen* cg2 = snow_create_codegen(node);
			SnFunctionDescription* desc = snow_codegen_compile_description(cg2);
			VALUE key = snow_store_add(desc);
			ASM(mov_id, IMMEDIATE(key), RDI);
			CALL(snow_store_get);
			ASM(mov, RAX, RDI);
			CALL(snow_create_function_from_description);
			ASM(mov, R15, ADDRESS(RAX, offsetof(SnFunction, declaration_context)));
			break;
		}
			
		case SN_AST_RETURN:
		{
			/*
				Returns require a non-local label, so it is handled specially.
				The codegen context contains a list (as a linkbuffer) of offsets
				that want to jump to the return label, so when the body of the
				function is done, they can all be linked in-place.
			*/
			if (node->children[0]) {
				codegen_compile_node(cgx, (SnAstNode*)node->children[0]);
			} else {
				ASM(mov_id, IMMEDIATE(SN_NIL), RAX);
			}
			LabelRef ref = ASM(jmp, NULL);
			if (!cgx->returns) {
				cgx->returns = snow_create_linkbuffer(16);
			}
			snow_linkbuffer_push_data(cgx->returns, (byte*)&ref.offset, sizeof(ref.offset));
			
			break;
		}
		
		case SN_AST_BREAK:
		{
			TRAP(); // Not implemented yet :(
		}
		
		case SN_AST_CONTINUE:
		{
			TRAP(); // Not implemented yet :(
		}
		
		case SN_AST_SELF:
		{
			ASM(mov_rev, RAX, ADDRESS(R15, offsetof(SnContext, self)));
			break;
		}
		
		case SN_AST_LOCAL:
		{
			VALUE vsym = node->children[0];
			ASSERT(is_symbol(vsym));
			
			VALUE vidx = NULL;
			if (cgx->base.result->local_index_map)
			{
				vidx = snow_map_get(cgx->base.result->local_index_map, vsym);
			}
			
			if (vidx)
			{
				// local to this scope
				ASM(mov, R14, RDI);
				ASM(mov_id, IMMEDIATE(value_to_int(vidx)), RSI);
				CALL(snow_array_get);
			}
			else
			{
				// local to parent or injected scope
				ASM(mov, R15, RDI);
				ASM(mov_id, IMMEDIATE(vsym), RSI);
				CALL(snow_context_get_local_by_value);
			}
			break;
		}
		
		case SN_AST_MEMBER:
		{
			SnAstNode* self = (SnAstNode*)node->children[0];
			VALUE vsym = node->children[1];
			ASSERT(is_symbol(vsym));
			
			codegen_compile_node(cgx, self);
			ASM(mov, RAX, RDI);
			ASM(mov_id, IMMEDIATE(vsym), RSI);
			CALL(snow_get_member_by_value);
			break;
		}
		
		case SN_AST_LOCAL_ASSIGNMENT:
		{
			VALUE vsym = node->children[0];
			ASSERT(is_symbol(vsym));
			SnAstNode* val = (SnAstNode*)node->children[1];
			
			codegen_compile_node(cgx, val);
			
			VALUE vidx = NULL;
			if (cgx->base.result->local_index_map)
			{
				vidx = snow_map_get(cgx->base.result->local_index_map, vsym);
			}
			
			if (vidx)
			{
				// local to this scope
				ASM(mov, R14, RDI);
				ASM(mov_id, IMMEDIATE(value_to_int(vidx)), RSI);
				ASM(mov, RAX, RDX);
				CALL(snow_array_set);
			}
			else
			{
				// local to a parent or injected scope
				ASM(mov, R15, RDI);
				ASM(mov_id, IMMEDIATE(vsym), RSI);
				ASM(mov, RAX, RDX);
				CALL(snow_context_set_local_by_value);
			}
			break;
		}
		
		case SN_AST_MEMBER_ASSIGNMENT:
		{
			SnAstNode* member = (SnAstNode*)node->children[0];
			SnAstNode* self = (SnAstNode*)member->children[0];
			VALUE vsym = member->children[1];
			ASSERT(is_symbol(vsym));
			
			intx tmp_self = RESERVE_TMP();
			codegen_compile_node(cgx, self);
			ASM(mov, RAX, TEMPORARY(tmp_self));
			
			SnAstNode* val = (SnAstNode*)node->children[1];
			codegen_compile_node(cgx, val);
			ASM(mov, RAX, RDX);
			ASM(mov_rev, RDI, TEMPORARY(tmp_self));
			ASM(mov_id, IMMEDIATE(vsym), RSI);
			CALL(snow_set_member_by_value);
			break;
		}
		
		case SN_AST_IF_ELSE:
		{
			Label if_false = ASM(label);
			Label after = ASM(label);
			// compile condition
			codegen_compile_node(cgx, (SnAstNode*)node->children[0]);
			ASM(mov, RAX, RDI);
			CALL(snow_eval_truth);
			ASM(xor, RCX, RCX);
			ASM(cmp, RAX, RCX);
			LabelRef false_jump = ASM(j, CC_ZERO, &if_false);
			// was true, execute truth body
			if (node->children[1])
				codegen_compile_node(cgx, (SnAstNode*)node->children[1]);
			LabelRef after_jump = ASM(jmp, &after);
			ASM(bind, &if_false);
			// was false, execute false body
			if (node->children[2])
				codegen_compile_node(cgx, (SnAstNode*)node->children[2]);
			ASM(bind, &after);
			
			ASM(link, &false_jump);
			ASM(link, &after_jump);
			break;
		}
		
		case SN_AST_CALL:
		{
			intx tmp_self = RESERVE_TMP();
			intx tmp_function = RESERVE_TMP();
			
			SnAstNode* func = (SnAstNode*)node->children[0];
			if (func->type == SN_AST_MEMBER)
			{
				// member call
				SnAstNode* self = (SnAstNode*)func->children[0];
				codegen_compile_node(cgx, self);
				ASM(mov, RAX, TEMPORARY(tmp_self));
				
				VALUE vsym = func->children[1];
				ASSERT(is_symbol(vsym));
				ASM(mov, RAX, RDI);
				ASM(mov_id, IMMEDIATE(vsym), RSI);
				CALL(snow_get_member_by_value);
				ASM(mov, RAX, TEMPORARY(tmp_function));
			}
			else
			{
				// closure call
				ASM(xor, RAX, RAX);
				ASM(mov, RAX, TEMPORARY(tmp_self));
				codegen_compile_node(cgx, func);
				ASM(mov, RAX, TEMPORARY(tmp_function));
			}
			
			// compile arguments
			intx tmp_args = RESERVE_TMP();
			SnAstNode* args_seq = (SnAstNode*)node->children[1];
			if (args_seq)
			{
				ASSERT(args_seq->type == SN_AST_SEQUENCE);
				SnArray* args = (SnArray*)args_seq->children[0];
				ASSERT(args->base.base.type == SN_ARRAY_TYPE);

				uintx num_args = snow_array_size(args);
				ASM(mov_id, IMMEDIATE(num_args), RDI);
				CALL(snow_create_arguments_with_size);
				ASM(mov, RAX, TEMPORARY(tmp_args));
				
				// TODO: Handle named args

				for (uintx i = 0; i < num_args; ++i) {
					SnAstNode* arg = (SnAstNode*)snow_array_get(args, i);
					codegen_compile_node(cgx, arg);
					ASM(mov, RAX, RSI);
					ASM(mov_rev, RDI, TEMPORARY(tmp_args));
					CALL(snow_arguments_push);
				}
				
				ASM(mov_rev, RDX, TEMPORARY(tmp_args));
			} else {
				ASM(xor, RDX, RDX);
				ASM(mov, RAX, TEMPORARY(tmp_args));
			}
			
			ASM(mov_rev, RDI, TEMPORARY(tmp_self));
			ASM(mov_rev, RSI, TEMPORARY(tmp_function));
			// args moved into RDX above
			CALL(snow_call_with_args);
			
			FREE_TMP(tmp_args);
			FREE_TMP(tmp_self);
			FREE_TMP(tmp_function);
			break;
		}
		case SN_AST_LOOP:
		{
			TRAP(); // Not implemented
			break;
		}
		case SN_AST_AND:
		case SN_AST_OR:
		case SN_AST_XOR:
			TRAP(); // logical expressions not implemented yet
			break;
		case SN_AST_NOT:
		{
			Label if_false = ASM(label);
			Label after = ASM(label);
		
			codegen_compile_node(cgx, node->children[0]);
			ASM(mov, RAX, RDI);
			CALL(snow_eval_truth);
			ASM(xor, RCX, RCX);
			ASM(cmp, RAX, RCX);
			LabelRef false_jump = ASM(j, CC_ZERO, &if_false);
			ASM(mov_id, IMMEDIATE(SN_FALSE), RAX);
			LabelRef after_jump = ASM(jmp, &after);
			ASM(bind, &if_false);
			ASM(mov_id, IMMEDIATE(SN_TRUE), RAX);
			ASM(bind, &after);
			
			ASM(link, &false_jump);
			ASM(link, &after_jump);
			break;
		}
	}
}
