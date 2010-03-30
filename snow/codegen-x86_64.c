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
#include "snow/task-intern.h"
#include "snow/exception-intern.h"

#include <stddef.h>

typedef struct SnCodegenInternal {
	SnLinkBuffer* returns;
	intx num_temporaries;
	SnArray* tmp_freelist;
} SnCodegenInternal;

static void codegen_compile_node(SnCodegen* cg, SnAstNode*);
static intx codegen_reserve_tmp(SnCodegenInternal* cgx);
static void codegen_free_tmp(SnCodegenInternal* cgx, intx tmp);

SnCodegen* snow_create_codegen(SnAstNode* root, SnCodegen* parent)
{
	ASSERT(root->node_type == SN_AST_FUNCTION && "Only function definitions can be used as root nodes in Snow ASTs.");
	
	SnCodegen* codegen = SNOW_GC_ALLOC_OBJECT(SnCodegen);
	
	codegen_init(codegen, root, parent);
	codegen->internal = (SnCodegenInternal*)snow_malloc(sizeof(SnCodegenInternal));
	
	codegen->internal->returns = NULL;
	codegen->internal->num_temporaries = 0;
	codegen->internal->tmp_freelist = NULL;
	
	return codegen;
}

void SnCodegen_finalize(SnCodegen* cg) {
	snow_free(cg->internal);
}

intx codegen_reserve_tmp(SnCodegenInternal* cgx)
{
	if (cgx->tmp_freelist && snow_array_size(cgx->tmp_freelist) > 0)
		return snow_value_to_int(snow_array_pop(cgx->tmp_freelist));
	else
		return cgx->num_temporaries++;
}

void codegen_free_tmp(SnCodegenInternal* cgx, intx tmp)
{
	if (!cgx->tmp_freelist)
		cgx->tmp_freelist = snow_create_array_with_size(32);
	snow_array_push(cgx->tmp_freelist, snow_int_to_value(tmp));
}

#define ASM(instr, ...) asm_##instr(cg->buffer, __VA_ARGS__)
#define ASM_S(instr) asm_##instr(cg->buffer)
#define ASM_LABEL ASM_S(label)
#define RESERVE_TMP() codegen_reserve_tmp(cg->internal)
#define FREE_TMP(tmp) codegen_free_tmp(cg->internal, tmp)
#define TEMPORARY(tmp) ADDRESS(RBP, -(tmp+1) * sizeof(VALUE))
#define CALL(func) codegen_compile_call_with_inlining(cg, (void(*)())(func))

static void codegen_compile_call_with_inlining(SnCodegen* cg, void(*func)())
{
	if (func == (void(*)())snow_eval_truth)
	{
		Label was_true = ASM_LABEL;
		
		uintx mask = ~(uintx)0x6;
		// if RDI has bits set besides for 0x6, it's true for certain
		ASM(mov, RDI, RAX);
		ASM(mov_id, IMMEDIATE(mask), RCX);
		ASM(and, RCX, RAX);
		ASM(xor, RCX, RCX);
		ASM(cmp, RAX, RCX);
		LabelRef was_true_jmp1 = ASM(j, CC_NOT_ZERO, &was_true);
		ASM(mov, RDI, RAX);
		ASM(mov_id, IMMEDIATE(SN_TRUE), RCX);
		ASM(cmp, RAX, RCX);
		LabelRef was_true_jmp2 = ASM(j, CC_EQUAL, &was_true);
		ASM(xor, RAX, RAX);
		ASM(bind, &was_true);
		
		ASM(link, &was_true_jmp1);
		ASM(link, &was_true_jmp2);
	}
	/*else if (func == (void(*)())snow_array_get)
	{
		// &array.a => %rdi
		// index => %rsi
		// %rdi->data => %r8
		ASM(lea, RDI, ADDRESS(RDI, offsetof(SnArray, a)));
		
		#ifdef DEBUG
		// Only do bounds checking in debug
		ASM(xor, RAX, RAX);
		Label out_of_bounds = ASM_LABEL; // predeclaration
		ASM(mov_rev, R8, ADDRESS(RDI, offsetof(struct array_t, size)));
		// we're loading quadwords (because of limited asm support), so mask the results
		ASM(mov_id, IMMEDIATE(0x00000000ffffffff), R9);
		ASM(and, R9, R8);
		ASM(cmp, R8, RSI);
		LabelRef out_of_bounds_jmp = ASM(j, CC_GREATER_EQUAL, &out_of_bounds);
		#endif // DEBUG
		
		// not out of bounds, go ahead
		ASM(mov_rev, RAX, ADDRESS(RDI, offsetof(struct array_t, data)));
		// TODO: If we get SIB addressing one day, this is a good candidate
		ASM(imul_i, RSI, RSI, IMMEDIATE(sizeof(VALUE)));
		ASM(add, RSI, RAX);
		ASM(mov_rev, RAX, ADDRESS(RAX, 0));
		
		#ifdef DEBUG
		ASM(bind, &out_of_bounds);
		ASM(link, &out_of_bounds_jmp);
		#endif // DEBUG
	}*/
	else
	{
		// no inline version for this codegen
		ASM(mov_id, IMMEDIATE(func), R10);
		ASM(call, R10);
	}
}

void codegen_compile_root(SnCodegen* cg)
{
	SnCodegenInternal* cgx = cg->internal;
	ASSERT(cg->root->node_type == SN_AST_FUNCTION);
	
	// prelude
	ASM(push, RBP);
	ASM(mov, RSP, RBP);
	int32_t stack_size_offset = ASM(sub_id, 0, RSP);
	ASM(push, R13);
	ASM(push, R14);
	ASM(mov, RDI, R13);                                           // function context in r13
	ASM(mov_rev, R14, ADDRESS(R13, offsetof(SnContext, locals))); // locals array in r14
	
	// add argument names to list of locals
	SnAstNode* args_seq = (SnAstNode*)cg->root->child2;
	if (args_seq)
	{
		ASSERT(args_seq->node_type == SN_AST_SEQUENCE);
		SnArray* args_array = (SnArray*)args_seq->child0;
		ASSERT_TYPE(args_array, SnArrayType);
		cg->result->argument_names = args_array;
		for (uintx i = 0; i < snow_array_size(args_array); ++i) {
			VALUE vsym = snow_array_get(args_array, i);
			ASSERT(snow_is_symbol(vsym));
			SnSymbol sym = snow_value_to_symbol(vsym);
			snow_function_description_define_local(cg->result, sym);
		}
	}
	
	// always clear rax before body, so empty functions will return nil.
	ASM(xor, RAX, RAX);
	
	// body
	SnAstNode* body_seq = (SnAstNode*)cg->root->child3;
	ASSERT(body_seq->node_type == SN_AST_SEQUENCE);
	codegen_compile_node(cg, body_seq);
	
	// return
	Label return_label = ASM_LABEL;
	ASM(bind, &return_label);
	ASM(pop, R14);
	ASM(pop, R13);
	ASM_S(leave);
	ASM_S(ret);
	
	// link all returns
	if (cg->internal->returns) {
		uintx bytes = snow_linkbuffer_size(cg->internal->returns);
		uintx len = bytes / sizeof(int32_t);
		int32_t offsets[bytes];
		snow_linkbuffer_copy_data(cg->internal->returns, offsets, bytes);
		
		for (uintx i = 0; i < len; ++i) {
			LabelRef ref;
			ref.offset = offsets[i];
			ref.label = &return_label;
			ASM(link, &ref);
		}
	}
	
	// link stack size
	uintx stack_size = cg->internal->num_temporaries * sizeof(VALUE);
	stack_size += (stack_size & 0xf); // alignment
	ASSERT(stack_size % 0x10 == 0);
	snow_linkbuffer_modify(cg->buffer, stack_size_offset, 4, (byte*)&stack_size);
	
	
	if (cg->internal->tmp_freelist && snow_array_size(cg->internal->tmp_freelist) != cg->internal->num_temporaries)
	{
		ASSERT(false && "Unfreed temporaries! You likely have a temporary register leak, which will yield suboptimal stack space usage.");
	}
}

void codegen_compile_node(SnCodegen* cg, SnAstNode* node)
{
	ASSERT(node->type == SnAstNodeType);
	
	switch (node->node_type) {
		case SN_AST_LITERAL:
		{
			VALUE val = node->child0;
			if (snow_is_object(val))
			{
				VALUE key = snow_store_add(val);
				ASM(mov_id, IMMEDIATE(key), RDI);
				CALL(snow_store_get);
			}
			else
			{
				ASM(mov_id, IMMEDIATE(node->child0), RAX);
			}
			break;
		}
		
		case SN_AST_SEQUENCE:
		{
			SnArray* seq = (SnArray*)node->child0;
			ASSERT_TYPE(seq, SnArrayType);
			for (uintx i = 0; i < snow_array_size(seq); ++i) {
				codegen_compile_node(cg, (SnAstNode*)snow_array_get(seq, i));
			}
			break;
		}
		
		case SN_AST_FUNCTION:
		{
			SnCodegen* cg2 = snow_create_codegen(node, cg);
			SnFunctionDescription* desc = snow_codegen_compile_description(cg2);
			VALUE key = snow_store_add(desc);
			ASM(mov_id, IMMEDIATE(key), RDI);
			CALL(snow_store_get);
			ASM(mov, RAX, RDI);
			CALL(snow_create_function_from_description);
			ASM(mov, RAX, RBX); // preserve
			ASM(mov, RAX, RDI);
			ASM(mov, R13, RSI);
			CALL(snow_function_declared_in_context);
			ASM(mov, RBX, RAX);
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
			if (node->child0) {
				codegen_compile_node(cg, (SnAstNode*)node->child0);
			} else {
				ASM(mov_id, IMMEDIATE(SN_NIL), RAX);
			}
			LabelRef ref = ASM(jmp, NULL);
			if (!cg->internal->returns) {
				cg->internal->returns = snow_create_linkbuffer(16);
			}
			snow_linkbuffer_push_data(cg->internal->returns, (byte*)&ref.offset, sizeof(ref.offset));
			
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
			ASM(mov_rev, RAX, ADDRESS(R13, offsetof(SnContext, self)));
			break;
		}
		
		case SN_AST_CURRENT_SCOPE:
		{
			ASM(mov, R13, RAX);
			break;
		}
		
		case SN_AST_LOCAL:
		{
			VALUE vsym = node->child0;
			ASSERT(snow_is_symbol(vsym));
			SnSymbol sym = snow_value_to_symbol(vsym);
			
			intx idx = snow_function_description_get_local_index(cg->result, sym);
			
			if (idx >= 0)
			{
				// local to this scope
				ASM(mov_rev, RDI, ADDRESS(R13, offsetof(SnContext, locals)));
				//ASM(mov, R14, RDI);
				ASM(mov_id, IMMEDIATE(idx), RSI);
				CALL(snow_array_get);
			}
			else
			{
				// check static scopes
				uint32_t reference_index;
				if (codegen_variable_reference(cg, sym, &reference_index))
				{
					// yes, it's in static scope
					ASM(mov_rev, RDI, ADDRESS(R13, offsetof(SnContext, function)));
					ASM(mov_id, reference_index, RSI);
					CALL(snow_function_get_referenced_variable);
				}
				else
				{
					// not in static scopes, defer to globals
					ASM(mov_id, IMMEDIATE(sym), RDI);
					ASM(mov, R13, RSI);
					CALL(snow_get_unspecified_local_from_context);
				}
			}
			
			break;
		}
		
		case SN_AST_LOCAL_ASSIGNMENT:
		{
			VALUE vsym = node->child0;
			ASSERT(snow_is_symbol(vsym));
			SnSymbol sym = snow_value_to_symbol(vsym);
			
			SnAstNode* val = (SnAstNode*)node->child1;
			
			intx idx = snow_function_description_get_local_index(cg->result, sym);
			
			if (idx >= 0)
			{
				// local already exists in scope
				// assign
				codegen_compile_node(cg, val);
				// result is in RAX now
				ASM(mov_rev, RDI, ADDRESS(R13, offsetof(SnContext, locals)));
				//ASM(mov, R14, RDI);
				ASM(mov_id, IMMEDIATE(idx), RSI);
				ASM(mov, RAX, RDX);
				CALL(snow_array_set);
			}
			else
			{
				// check static scopes
				uint32_t reference_index;
				if (codegen_variable_reference(cg, sym, &reference_index))
				{
					// yea, it's in static scope
					codegen_compile_node(cg, val);
					// result is in RAX now
					ASM(mov_rev, RDI, ADDRESS(R13, offsetof(SnContext, function)));
					ASM(mov_id, reference_index, RSI);
					ASM(mov, RAX, RDX);
					CALL(snow_function_set_referenced_variable);
				}
				else
				{
					// not in static scope, so either
					if (cg->parent)
					{
						// we have parents, so this isn't global scope, so create it
						// It is important that snow_function_description_define_local is called before codegen_compile_node,
						// in the case where what's being assigned references itself.
						idx = snow_function_description_define_local(cg->result, sym);
						codegen_compile_node(cg, val);
						// result is in RAX now
						ASSERT(idx >= 0);
						//ASM(mov, R14, RDI);
						ASM(mov_rev, RDI, ADDRESS(R13, offsetof(SnContext, locals)));
						ASM(mov_id, IMMEDIATE(idx), RSI);
						ASM(mov, RAX, RDX);
						CALL(snow_array_set);
					}
					else
					{
						// we have no parents, so we are in global scope, so set that
						codegen_compile_node(cg, val);
						// result is in RAX now
						ASM(mov_id, IMMEDIATE(sym), RDI);
						ASM(mov, RAX, RSI);
						ASM(mov, R13, RDX);
						CALL(snow_set_global_from_context);
					}
				}
			}
			
			// notify object of assignment
			ASM(mov, RAX, RDI);
			ASM(mov_id, IMMEDIATE(sym), RSI);
			ASM(xor, RDX, RDX);
			CALL(snow_set_object_assigned);
			
			break;
		}
		
		case SN_AST_MEMBER:
		{
			SnAstNode* self = (SnAstNode*)node->child0;
			VALUE vsym = node->child1;
			ASSERT(snow_is_symbol(vsym));
			
			codegen_compile_node(cg, self);
			ASM(mov, RAX, RDI);
			ASM(mov_id, IMMEDIATE(snow_value_to_symbol(vsym)), RSI);
			CALL(snow_get_member_with_fallback);
			break;
		}
		
		case SN_AST_MEMBER_ASSIGNMENT:
		{
			SnAstNode* member = (SnAstNode*)node->child0;
			SnAstNode* self = (SnAstNode*)member->child0;
			VALUE vsym = member->child1;
			ASSERT(snow_is_symbol(vsym));
			
			intx tmp_self = RESERVE_TMP();
			codegen_compile_node(cg, self);
			ASM(mov, RAX, TEMPORARY(tmp_self));
			
			SnAstNode* val = (SnAstNode*)node->child1;
			codegen_compile_node(cg, val);
			ASM(mov, RAX, RDX);
			ASM(mov_rev, RDI, TEMPORARY(tmp_self));
			ASM(mov_id, IMMEDIATE(snow_value_to_symbol(vsym)), RSI);
			CALL(snow_set_member);
			
			// notify object of assignment
			ASM(mov, RAX, RDI);
			ASM(mov_id, IMMEDIATE(snow_value_to_symbol(vsym)), RSI);
			ASM(mov_rev, RDX, TEMPORARY(tmp_self));
			CALL(snow_set_object_assigned);
			
			FREE_TMP(tmp_self);
			break;
		}
		
		case SN_AST_IF_ELSE:
		{
			Label if_false = ASM_LABEL;
			Label after = ASM_LABEL;
			// compile condition
			codegen_compile_node(cg, (SnAstNode*)node->child0);
			ASM(mov, RAX, RDI);
			CALL(snow_eval_truth);
			ASM(xor, RCX, RCX);
			ASM(cmp, RAX, RCX);
			LabelRef false_jump = ASM(j, CC_ZERO, &if_false);
			// was true, execute truth body
			if (node->child1)
				codegen_compile_node(cg, (SnAstNode*)node->child1);
			LabelRef after_jump = ASM(jmp, &after);
			ASM(bind, &if_false);
			// was false, execute false body
			if (node->child2)
				codegen_compile_node(cg, (SnAstNode*)node->child2);
			ASM(bind, &after);
			
			ASM(link, &false_jump);
			ASM(link, &after_jump);
			break;
		}
		
		case SN_AST_CALL:
		{
			intx tmp_self = RESERVE_TMP();
			intx tmp_function = RESERVE_TMP();
			
			SnAstNode* func = (SnAstNode*)node->child0;
			if (func->node_type == SN_AST_MEMBER)
			{
				// member call
				SnAstNode* self = (SnAstNode*)func->child0;
				codegen_compile_node(cg, self);
				ASM(mov, RAX, TEMPORARY(tmp_self));
				
				VALUE vsym = func->child1;
				ASSERT(snow_is_symbol(vsym));
				ASM(mov, RAX, RDI);
				ASM(mov_id, IMMEDIATE(snow_value_to_symbol(vsym)), RSI);
				CALL(snow_get_member_for_method_call);
				ASM(mov, RAX, TEMPORARY(tmp_function));
			}
			else
			{
				// closure call
				ASM(xor, RAX, RAX);
				ASM(mov, RAX, TEMPORARY(tmp_self));
				codegen_compile_node(cg, func);
				ASM(mov, RAX, TEMPORARY(tmp_function));
			}
			
			// compile arguments
			intx tmp_args = RESERVE_TMP();
			SnAstNode* args_seq = (SnAstNode*)node->child1;
			if (args_seq)
			{
				ASSERT(args_seq->node_type == SN_AST_SEQUENCE);
				SnArray* args = (SnArray*)args_seq->child0;
				ASSERT_TYPE(args, SnArrayType);

				uintx num_args = snow_array_size(args);
				ASM(mov_id, IMMEDIATE(num_args), RDI);
				CALL(snow_create_arguments_with_size);
				ASM(mov, RAX, TEMPORARY(tmp_args));
				
				// TODO: Handle named args

				for (uintx i = 0; i < num_args; ++i) {
					SnAstNode* arg = (SnAstNode*)snow_array_get(args, i);
					
					if (arg->node_type == SN_AST_LOCAL_ASSIGNMENT)
					{
						VALUE vname = arg->child0;
						ASSERT(snow_is_symbol(vname));
						SnSymbol name = snow_value_to_symbol(vname);
						SnAstNode* val = (SnAstNode*)arg->child1;
						// named argument
						codegen_compile_node(cg, val);
						ASM(mov_id, IMMEDIATE(name), RSI);
						ASM(mov, RAX, RDX);
						ASM(mov_rev, RDI, TEMPORARY(tmp_args));
						CALL(snow_arguments_push_named);
					}
					else
					{
						// normal argument
						codegen_compile_node(cg, arg);
						ASM(mov, RAX, RSI);
						ASM(mov_rev, RDI, TEMPORARY(tmp_args));
						CALL(snow_arguments_push);
					}
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
			Label cond = ASM_LABEL;
			Label after = ASM_LABEL;
			
			ASM(bind, &cond);
			codegen_compile_node(cg, (SnAstNode*)node->child0);
			ASM(mov, RAX, RDI);
			CALL(snow_eval_truth);
			ASM(xor, RCX, RCX);
			ASM(cmp, RAX, RCX);
			LabelRef after_jmp = ASM(j, CC_ZERO, &after);
			codegen_compile_node(cg, (SnAstNode*)node->child1);
			LabelRef cond_jmp = ASM(jmp, &cond);
			ASM(bind, &after);
			
			ASM(link, &after_jmp);
			ASM(link, &cond_jmp);
			break;
		}
		case SN_AST_TRY:
		{
			SnAstNode* catch_node = node->child1;
			
			Label catch = ASM_LABEL;
			Label ensure = ASM_LABEL;
			Label skip_propagation = ASM_LABEL;
			LabelRef inner_ensure_jmp;
			
			intx return_value = RESERVE_TMP();
			intx exception_handler = RESERVE_TMP();
			intx exception_to_propagate = RESERVE_TMP();
			
			ASM(mov_id, IMMEDIATE(0), TEMPORARY(exception_to_propagate));
			ASM(mov_id, IMMEDIATE(SN_NIL), TEMPORARY(return_value));
			
			CALL(snow_create_exception_handler);
			ASM(mov, RAX, TEMPORARY(exception_handler));
			
			ASM(mov, RAX, RDI); // RDI <- exception_handler
			CALL(snow_push_exception_handler);
			
			ASM(mov_rev, RDI, TEMPORARY(exception_handler));
			ASM(add_id, IMMEDIATE(offsetof(SnExceptionHandler, state)), RDI);
			CALL(snow_save_execution_state);
			
			ASM(xor, RCX, RCX);
			ASM(cmp, RAX, RCX);
			LabelRef catch_jmp = ASM(j, CC_NOT_EQUAL, &catch);
			
			codegen_compile_node(cg, (SnAstNode*)node->child0);
			ASM(mov, RAX, TEMPORARY(return_value));
			LabelRef outer_ensure_jmp = ASM(jmp, &ensure);
			
			ASM(bind, &catch);
			if (catch_node) {
				Label catch_catch = ASM_LABEL;
				LabelRef catch_condition_failed_jmp;
				intx catch_exception_handler = RESERVE_TMP();
				
				CALL(snow_create_exception_handler);
				ASM(mov, RAX, TEMPORARY(catch_exception_handler));
				
				// `catch_exception_handler->exception = exception_handler->exception;`
				ASM(mov_rev, R10, TEMPORARY(exception_handler));
				ASM(mov_rev, R11, ADDRESS(R10, offsetof(SnExceptionHandler, exception)));
				ASM(mov, R11, ADDRESS(RAX, offsetof(SnExceptionHandler, exception)));
				
				ASM(mov_rev, RDI, TEMPORARY(catch_exception_handler));
				CALL(snow_push_exception_handler);
				
				ASM(mov_rev, RDI, TEMPORARY(catch_exception_handler));
				ASM(add_id, IMMEDIATE(offsetof(SnExceptionHandler, state)), RDI);
				CALL(snow_save_execution_state);
				
				ASM(xor, RCX, RCX);
				ASM(cmp, RAX, RCX);
				LabelRef catch_catch_jmp = ASM(j, CC_NOT_EQUAL, &catch_catch);
				
				if (catch_node->child0) {
					// `<identifier> = Exception.current`
					// TODO: Do this tree-rewrite in parser.yy.
					SnSymbol identifier = snow_value_to_symbol(catch_node->child0);
					SnAstNode* exception_getter = snow_ast_member(snow_ast_local(snow_symbol("Exception")),
					                                              snow_symbol("current"));
					SnAstNode* assignment = snow_ast_local_assign(identifier, exception_getter);
					
					codegen_compile_node(cg, assignment);
				}
				
				if (catch_node->child1) {
					codegen_compile_node(cg, (SnAstNode*)catch_node->child1);
					ASM(mov, RAX, RDI);
					CALL(snow_eval_truth);
					ASM(xor, RCX, RCX);
					ASM(cmp, RAX, RCX);
					catch_condition_failed_jmp = ASM(j, CC_ZERO, &catch_catch);
				}
				
				codegen_compile_node(cg, (SnAstNode*)catch_node->child2);
				ASM(mov, RAX, TEMPORARY(return_value));
				CALL(snow_pop_exception_handler);
				inner_ensure_jmp = ASM(jmp, &ensure);
				
				ASM(bind, &catch_catch);
				ASM(mov_rev, R10, TEMPORARY(catch_exception_handler));
				ASM(mov_rev, R11, ADDRESS(R10, offsetof(SnExceptionHandler, exception)));
				ASM(mov, R11, TEMPORARY(exception_to_propagate));
				CALL(snow_pop_exception_handler);
				
				ASM(link, &catch_catch_jmp);
				if (catch_node->child1) ASM(link, &catch_condition_failed_jmp);
				FREE_TMP(catch_exception_handler);
			} else {
				ASM(mov_rev, R10, TEMPORARY(exception_handler));
				ASM(mov_rev, R11, ADDRESS(R10, offsetof(SnExceptionHandler, exception)));
				ASM(mov, R11, TEMPORARY(exception_to_propagate));
			}
			
			ASM(bind, &ensure);
			ASM(mov_rev, RAX, TEMPORARY(exception_handler));
			ASM(mov_rev, RDI, ADDRESS(RAX, offsetof(SnExceptionHandler, previous)));
			
			// `exception_handler->previous->exception = exception_handler->exception;`
			ASM(mov_rev, R11, ADDRESS(RAX, offsetof(SnExceptionHandler, exception)));
			ASM(mov, R11, ADDRESS(RDI, offsetof(SnExceptionHandler, exception)));
			
			CALL(snow_pop_exception_handler);
			
			if (node->child2)
				codegen_compile_node(cg, (SnAstNode*)node->child2);
			
			ASM(mov_rev, RDI, TEMPORARY(exception_to_propagate));
			ASM(cmp_id, IMMEDIATE(0), RDI);
			LabelRef skip_propagation_jmp = ASM(j, CC_ZERO, &skip_propagation);
			CALL(snow_throw_exception);
			
			ASM(bind, &skip_propagation);
			CALL(snow_current_exception_handler);
			ASM(mov_id, IMMEDIATE(0), ADDRESS(RAX, offsetof(SnExceptionHandler, exception)));
			
			ASM(mov_rev, RAX, TEMPORARY(return_value));
			
			ASM(link, &catch_jmp);
			ASM(link, &outer_ensure_jmp);
			ASM(link, &skip_propagation_jmp);
			if (catch_node) ASM(link, &inner_ensure_jmp);
			
			FREE_TMP(return_value);
			FREE_TMP(exception_handler);
			FREE_TMP(exception_to_propagate);
			break;
		}
		case SN_AST_CATCH:
			TRAP(); // A catch AST node should never be codegenned.
			break;
		case SN_AST_AND:
		{
			SnAstNode* left = (SnAstNode*)node->child0;
			SnAstNode* right = (SnAstNode*)node->child1;
			
			intx left_save = RESERVE_TMP();
			Label was_false = ASM_LABEL;
			
			codegen_compile_node(cg, left);
			ASM(mov, RAX, TEMPORARY(left_save));
			ASM(mov, RAX, RDI);
			CALL(snow_eval_truth);
			ASM(cmp_id, IMMEDIATE(0), RAX);
			ASM(mov_rev, RAX, TEMPORARY(left_save));
			FREE_TMP(left_save);
			LabelRef jump_false = ASM(j, CC_EQUAL, &was_false);
			codegen_compile_node(cg, right);
			ASM(bind, &was_false);
			
			ASM(link, &jump_false);
			break;
		}
		case SN_AST_OR:
		{
			SnAstNode* left = (SnAstNode*)node->child0;
			SnAstNode* right = (SnAstNode*)node->child1;
			
			intx left_save = RESERVE_TMP();
			Label was_true = ASM_LABEL;
			
			codegen_compile_node(cg, left);
			ASM(mov, RAX, TEMPORARY(left_save));
			ASM(mov, RAX, RDI);
			CALL(snow_eval_truth);
			ASM(cmp_id, IMMEDIATE(0), RAX);
			ASM(mov_rev, RAX, TEMPORARY(left_save));
			FREE_TMP(left_save);
			LabelRef jump_true = ASM(j, CC_NOT_EQUAL, &was_true);
			codegen_compile_node(cg, right);
			ASM(bind, &was_true);
			
			ASM(link, &jump_true);
			break;
		}
		case SN_AST_XOR:
		{
			SnAstNode* left = (SnAstNode*)node->child0;
			SnAstNode* right = (SnAstNode*)node->child1;
			
			// TODO: Optimize this.
			Label both_true_or_false = ASM_LABEL;
			Label left_true = ASM_LABEL;
			Label done = ASM_LABEL;
			intx left_save = RESERVE_TMP();
			intx right_save = RESERVE_TMP();
			codegen_compile_node(cg, left);
			ASM(mov, RAX, TEMPORARY(left_save));
			codegen_compile_node(cg, right);
			ASM(mov, RAX, TEMPORARY(right_save));
			ASM(mov, RAX, RDI);
			CALL(snow_eval_truth);
			ASM(mov, RAX, RBX); // RBX is preserved
			ASM(mov_rev, RDI, TEMPORARY(left_save));
			CALL(snow_eval_truth);
			
			// compare truthiness in RAX and RBX
			ASM(cmp, RAX, RBX);
			LabelRef both_ref = ASM(j, CC_EQUAL, &both_true_or_false);
			
			// find out which was true
			ASM(cmp_id, IMMEDIATE(0), RAX);
			LabelRef left_ref = ASM(j, CC_NOT_EQUAL, &left_true);
			ASM(mov_rev, RAX, TEMPORARY(right_save));
			LabelRef done_ref1 = ASM(jmp, &done);
			
			ASM(bind, &left_true);
			ASM(mov_rev, RAX, TEMPORARY(left_save));
			LabelRef done_ref2 = ASM(jmp, &done);
			
			ASM(bind, &both_true_or_false);
			ASM(mov_id, IMMEDIATE(SN_FALSE), RAX);
			
			ASM(bind, &done);
			
			// link references
			ASM(link, &both_ref);
			ASM(link, &left_ref);
			ASM(link, &done_ref1);
			ASM(link, &done_ref2);
			break;
		}
		case SN_AST_NOT:
		{
			Label if_false = ASM_LABEL;
			Label after = ASM_LABEL;
		
			codegen_compile_node(cg, node->child0);
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
		case SN_AST_PARALLEL_THREAD:
		case SN_AST_PARALLEL_FORK:
		{
			SnAstNode* seq = node->child0;
			ASSERT(seq->node_type == SN_AST_SEQUENCE);
			SnArray* seq_array = (SnArray*)seq->child0;
			ASSERT_TYPE(seq_array, SnArrayType);
			
			SnArray* functions = snow_create_array_with_size(snow_array_size(seq_array));
			
			for (uintx i = 0; i < snow_array_size(seq_array); ++i) {
				SnAstNode* subfunc_body = snow_ast_sequence(1, snow_array_get(seq_array, i));
				SnAstNode* subfunc = snow_ast_function("<parallellized chunk>", "<TODO>", NULL, subfunc_body);
				SnCodegen* cg2 = snow_create_codegen(subfunc, cg);
				SnFunctionDescription* desc = snow_codegen_compile_description(cg2);
				snow_array_push(functions, desc);
			}
			
			VALUE key = snow_store_add(functions);
			
			ASM(mov_id, IMMEDIATE(key), RDI);
			CALL(snow_store_get);
			ASM(mov, RAX, RDI);
			ASM(mov, R13, RSI);
			if (node->node_type == SN_AST_PARALLEL_THREAD) {
				CALL(snow_invoke_parallel_threads);
			} else if (node->node_type == SN_AST_PARALLEL_FORK) {
				CALL(snow_invoke_parallel_forks);
			} else {
				TRAP(); // wtf
			}
			
			break;
		}
	}
}
