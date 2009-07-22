#include "snow/codegen.h"
#include "snow/snow.h"
#include "snow/intern.h"
#include "snow/linkbuffer.h"
#include "snow/asm-x86_64.h"
#include "snow/array.h"
#include "snow/gc.h"
#include "snow/context.h"
#include "snow/array.h"
#include "snow/arguments.h"

#include <sys/mman.h>
#include <stddef.h>

typedef struct SnCodegenX {
	SnCodegen base;
	SnFunction* result;
	SnLinkBuffer* buffer;
	SnLinkBuffer* returns;
	intx num_temporaries;
	SnArray* tmp_freelist;
} SnCodegenX;

static void codegen_free(VALUE);
static void codegen_compile_root(SnCodegenX* cgx);
static void codegen_compile_node(SnCodegenX* cgx, SnAstNode*);
static intx codegen_reserve_tmp(SnCodegenX* cgx);
static void codegen_free_tmp(SnCodegenX* cgx, intx tmp);

SnCodegen* snow_create_codegen(SnAstNode* root)
{
	ASSERT(root->type == SN_AST_FUNCTION && "Only function definitions can be used as root nodes in Snow ASTs.");
	SnCodegenX* codegen = (SnCodegenX*)snow_alloc_any_object(SN_CODEGEN_TYPE, sizeof(SnCodegenX));
	snow_gc_set_free_func(codegen, codegen_free);
	codegen->base.root = root;
	codegen->buffer = snow_create_linkbuffer(1024);
	codegen->returns = NULL;
	codegen->num_temporaries = 0;
	codegen->tmp_freelist = NULL;
	return (SnCodegen*)codegen;
}

static void codegen_free(VALUE cg)
{
	SnCodegenX* cgx = (SnCodegenX*)cg;
	snow_free_linkbuffer(cgx->buffer);
}

SnFunction* snow_codegen_compile(SnCodegen* cg)
{
	SnCodegenX* cgx = (SnCodegenX*)cg;
	cgx->result = snow_create_function(NULL);
	
	codegen_compile_root(cgx);
	
	uintx len = snow_linkbuffer_size(cgx->buffer);
	
	// FIXME: Use an executable allocator so we can have as many functions as possible in one page of memory.
	byte* compiled_code = (byte*)valloc(len);
	snow_linkbuffer_copy_data(cgx->buffer, compiled_code, len);
	mprotect(compiled_code, len, PROT_EXEC);
//	debug("COMPILED CODE AT 0x%llx\n", compiled_code);
	
	cgx->result->func = (SnFunctionPtr)compiled_code;
	
	return cgx->result;
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

#define ASM(instr, ...) asm_##instr(cgx->buffer, ##__VA_ARGS__)
#define RESERVE_TMP() codegen_reserve_tmp(cgx)
#define FREE_TMP(tmp) codegen_free_tmp(cgx, tmp)
#define TEMPORARY(tmp) ADDRESS(RBP, -(tmp+1) * sizeof(VALUE))
#define CALL(func) ASM(mov_id, IMMEDIATE(func), RCX); ASM(call, RCX);

void codegen_compile_root(SnCodegenX* cgx)
{
	ASSERT(cgx->base.root->type == SN_AST_FUNCTION);
	
	// prelude
	ASM(push, RBP);
	ASM(mov, RSP, RBP);
	int32_t stack_size_offset = ASM(sub_id, 0, RSP);
	ASM(push, R15);
	ASM(push, R15); // stack padding
	ASM(mov, RDI, R15);  // function context is always in r15, since r15 is preserved across calls
	
	ASSERT(cgx->base.root->type == SN_AST_FUNCTION);
	
	// args
	SnAstNode* args_seq = (SnAstNode*)cgx->base.root->children[2];
	ASSERT(args_seq->type == SN_AST_SEQUENCE);
	SnArray* args_array = (SnArray*)args_seq->children[0];
	ASSERT(args_array->base.base.type == SN_ARRAY_TYPE);
	cgx->result->argument_names = args_array;
	#ifdef DEBUG
	for (uintx i = 0; i < args_array->size; ++i) {
		ASSERT(is_symbol(args_array->data[i]));
	}
	#endif
	
	// body
	SnAstNode* body_seq = (SnAstNode*)cgx->base.root->children[3];
	ASSERT(body_seq->type == SN_AST_SEQUENCE);
	codegen_compile_node(cgx, body_seq);
	
	// return
	Label return_label = ASM(label);
	ASM(bind, &return_label);
	ASM(pop, R15);
	ASM(pop, R15); // stack padding
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
	snow_linkbuffer_modify(cgx->buffer, stack_size_offset, 4, (byte*)&stack_size);
	
	
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
			for (uintx i = 0; i < seq->size; ++i) {
				codegen_compile_node(cgx, (SnAstNode*)seq->data[i]);
			}
			break;
		}
		
		case SN_AST_FUNCTION:
			ASM(mov_id, IMMEDIATE(node->children[0]), RAX);
			break;
			
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
			
			ASM(mov, R15, RDI);                     // context
			ASM(mov_id, IMMEDIATE(vsym), RSI);      // name of local as symbol
			CALL(snow_context_get_local_by_value);
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
			break;
		}
		
		case SN_AST_MEMBER_ASSIGNMENT:
		{
			break;
		}
		
		case SN_AST_IF_ELSE:
		{
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
			SnAstNode* args_seq = (SnAstNode*)node->children[1];
			ASSERT(args_seq->type == SN_AST_SEQUENCE);
			SnArray* args = (SnArray*)args_seq->children[0];
			ASSERT(args->base.base.type == SN_ARRAY_TYPE);
			
			uintx num_args = snow_array_size(args);
			intx tmp_args = RESERVE_TMP();
			ASM(mov_id, IMMEDIATE(num_args), RDI);
			CALL(snow_create_arguments_with_size);
			ASM(mov, RAX, TEMPORARY(tmp_args));
			
			// TODO: Handle named args
			
			for (uintx i = 0; i < num_args; ++i) {
				SnAstNode* arg = (SnAstNode*)args->data[i];
				codegen_compile_node(cgx, arg);
				ASM(mov, RAX, RSI);
				ASM(mov_rev, RDI, TEMPORARY(tmp_args));
				CALL(snow_arguments_push);
			}
			
			ASM(mov_rev, RDI, TEMPORARY(tmp_self));
			ASM(mov_rev, RSI, TEMPORARY(tmp_function));
			ASM(mov_rev, RDX, TEMPORARY(tmp_args));
			CALL(snow_call_with_args);
			
			FREE_TMP(tmp_args);
			FREE_TMP(tmp_self);
			FREE_TMP(tmp_function);
			break;
		}
		case SN_AST_LOOP:
		case SN_AST_AND:
		case SN_AST_OR:
		case SN_AST_XOR:
		case SN_AST_NOT:
		break;
	}
}
