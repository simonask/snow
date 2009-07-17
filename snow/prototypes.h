#ifndef PROTOTYPES_H_HFYBG82I
#define PROTOTYPES_H_HFYBG82I

#include "snow/object.h"

HIDDEN SnObject* create_object_prototype();
HIDDEN SnObject* create_class_prototype();
HIDDEN SnObject* create_continuation_prototype();
HIDDEN SnObject* create_context_prototype();
HIDDEN SnObject* create_arguments_prototype();
HIDDEN SnObject* create_function_prototype();
HIDDEN SnObject* create_string_prototype();
HIDDEN SnObject* create_array_prototype();
HIDDEN SnObject* create_map_prototype();
HIDDEN SnObject* create_wrapper_prototype();
HIDDEN SnObject* create_codegen_prototype();
HIDDEN SnObject* create_ast_prototype();
HIDDEN SnObject* create_integer_prototype();
HIDDEN SnObject* create_nil_prototype();
HIDDEN SnObject* create_boolean_prototype();
HIDDEN SnObject* create_symbol_prototype();
HIDDEN SnObject* create_float_prototype();

#endif /* end of include guard: PROTOTYPES_H_HFYBG82I */
