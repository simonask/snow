#include "snow/codegen.h"
#include "snow/intern.h"
#include "snow/linkbuffer.h"
#include "snow/asm-x86_64.h"
#include "snow/gc.h"

#include <sys/mman.h>

typedef struct SnCodegenX {
	SnCodegen base;
	SnLinkBuffer* buffer;
	uintx num_temporaries; // etc.
} SnCodegenX;

static void codegen_free(VALUE);
static void codegen_compile_node(SnCodegenX* cgx, SnAstNode*);

SnCodegen* snow_create_codegen(SnAstNode* root)
{
	ASSERT(root->type == SN_AST_FUNCTION && "Only function definitions can be used as root nodes in Snow ASTs.");
	SnCodegenX* codegen = (SnCodegenX*)snow_alloc_any_object(SN_CODEGEN_TYPE, sizeof(SnCodegenX));
	snow_gc_set_free_func(codegen, codegen_free);
	codegen->base.root = root;
	codegen->buffer = snow_create_linkbuffer(1024);
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
	codegen_compile_node(cgx, cgx->base.root);
	
	uintx len = snow_linkbuffer_size(cgx->buffer);
	byte* compiled_code = (byte*)valloc(len);
	snow_linkbuffer_copy_data(cgx->buffer, compiled_code, len);
	mprotect(compiled_code, len, PROT_EXEC);
	debug("COMPILED CODE AT 0x%llx\n", compiled_code);
	
	SnFunction* func = snow_create_function((SnContinuationFunc)compiled_code);
	TRAP();
	
	return func;
}

void codegen_compile_node(SnCodegenX* cgx, SnAstNode* node)
{
	switch (node->type) {
		case SN_AST_LITERAL:
			asm_mov_id(cgx->buffer, node->children[0], RAX);
			break;
		case SN_AST_SEQUENCE:
			break;
		case SN_AST_FUNCTION:
			asm_mov_id(cgx->buffer, node->children[0], RAX);
			break;
		case SN_AST_RETURN:
		case SN_AST_BREAK:
		case SN_AST_CONTINUE:
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
