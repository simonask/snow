#include "snow/codegen.h"
#include "snow/intern.h"
#include "snow/linkbuffer.h"
#include "snow/asm-x86_64.h"
#include "snow/array.h"
#include "snow/gc.h"

#include <sys/mman.h>


typedef struct SnCodegenX {
	SnCodegen base;
	SnLinkBuffer* buffer;
	SnLinkBuffer* returns;
	uintx num_temporaries; // etc.
} SnCodegenX;

static void codegen_free(VALUE);
static void codegen_compile_root(SnCodegenX* cgx);
static void codegen_compile_node(SnCodegenX* cgx, SnAstNode*);

SnCodegen* snow_create_codegen(SnAstNode* root)
{
	ASSERT(root->type == SN_AST_FUNCTION && "Only function definitions can be used as root nodes in Snow ASTs.");
	SnCodegenX* codegen = (SnCodegenX*)snow_alloc_any_object(SN_CODEGEN_TYPE, sizeof(SnCodegenX));
	snow_gc_set_free_func(codegen, codegen_free);
	codegen->base.root = root;
	codegen->buffer = snow_create_linkbuffer(1024);
	codegen->returns = NULL;
	codegen->num_temporaries = 0;
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
	
	codegen_compile_root(cgx);
	
	uintx len = snow_linkbuffer_size(cgx->buffer);
	byte* compiled_code = (byte*)valloc(len);
	snow_linkbuffer_copy_data(cgx->buffer, compiled_code, len);
	mprotect(compiled_code, len, PROT_EXEC);
	debug("COMPILED CODE AT 0x%llx\n", compiled_code);
	
	SnFunction* func = snow_create_function((SnContinuationFunc)compiled_code);
	TRAP();
	
	return func;
}

#define ASM(instr, ...) asm_##instr(cgx->buffer, ##__VA_ARGS__)
#define CURRENT_OFFSET() (offset)snow_linkbuffer_size(cgx->buffer)
#define WRITE_OFFSET(src, dst) 

void codegen_compile_root(SnCodegenX* cgx)
{
	ASSERT(cgx->base.root->type == SN_AST_FUNCTION);
	
	// compile function prelude/postlude
	
	codegen_compile_node(cgx, cgx->base.root);
	
	Label return_label = ASM(label);
	ASM(bind, &return_label);
	
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
		case SN_AST_LOCAL:
		case SN_AST_MEMBER:
		case SN_AST_LOCAL_ASSIGNMENT:
		case SN_AST_MEMBER_ASSIGNMENT:
		case SN_AST_IF_ELSE:
		case SN_AST_CALL:
		case SN_AST_LOOP:
		case SN_AST_AND:
		case SN_AST_OR:
		case SN_AST_XOR:
		case SN_AST_NOT:
		break;
	}
}
