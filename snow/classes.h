#ifndef PROTOTYPES_H_HFYBG82I
#define PROTOTYPES_H_HFYBG82I

#include "snow/class.h"

HIDDEN void init_object_class(SnClass* klass);
HIDDEN void init_class_class(SnClass* klass);
HIDDEN void init_continuation_class(SnClass* klass);
HIDDEN void init_context_class(SnClass* klass);
HIDDEN void init_arguments_class(SnClass* klass);
HIDDEN void init_function_class(SnClass* klass);
HIDDEN void init_string_class(SnClass* klass);
HIDDEN void init_array_class(SnClass* klass);
HIDDEN void init_map_class(SnClass* klass);
HIDDEN void init_wrapper_class(SnClass* klass);
HIDDEN void init_codegen_class(SnClass* klass);
HIDDEN void init_ast_class(SnClass* klass);
HIDDEN void init_integer_class(SnClass* klass);
HIDDEN void init_nil_class(SnClass* klass);
HIDDEN void init_boolean_class(SnClass* klass);
HIDDEN void init_symbol_class(SnClass* klass);
HIDDEN void init_float_class(SnClass* klass);

#endif /* end of include guard: PROTOTYPES_H_HFYBG82I */
