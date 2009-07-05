#include "snow/codegen.h"
#include "snow/intern.h"
#include "snow/linkbuffer.h"
#include "snow/asm-x86_64.h"
#include "snow/gc.h"

typedef struct SnCodegenX {
	SnCodegen base;
	SnLinkBuffer* buffer;
	uintx num_temporaries; // etc.
} SnCodegenX;

static void codegen_free(VALUE);

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
	return NULL;
}
