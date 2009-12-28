#ifndef SNOW_H_YMYJ9XZE
#define SNOW_H_YMYJ9XZE

#include "snow/basic.h"
#include "snow/symbol.h"
#include "snow/arguments.h"
#include "snow/class.h"
#include <stdarg.h>

struct SnFunction;
struct SnContext;
struct SnArray;

CAPI void snow_init();
CAPI VALUE snow_eval(const char* str);
CAPI VALUE snow_eval_in_context(const char* str, struct SnContext* context);
CAPI struct SnArray* snow_get_load_paths();
CAPI VALUE snow_load(const char* file);
CAPI VALUE snow_require(const char* file);

CAPI VALUE snow_call(VALUE self, VALUE closure, uintx num_args, ...);
CAPI VALUE snow_call_va(VALUE self, VALUE closure, uintx num_args, va_list* ap);
CAPI VALUE snow_call_with_args(VALUE self, VALUE closure, SnArguments* args)             ATTR_HOT;
CAPI VALUE snow_call_method(VALUE self, SnSymbol method, uintx num_args, ...)            ATTR_HOT;
CAPI VALUE snow_get_member(VALUE self, SnSymbol member)                                  ATTR_HOT;
CAPI VALUE snow_get_member_for_method_call(VALUE self, SnSymbol member)                  ATTR_HOT;
CAPI VALUE snow_get_member_with_fallback(VALUE self, SnSymbol member)                    ATTR_HOT; // will call .member_missing
CAPI VALUE snow_set_member(VALUE self, SnSymbol member, VALUE value)                     ATTR_HOT;
CAPI VALUE snow_get_global(SnSymbol name)                                                ATTR_HOT;
CAPI VALUE snow_get_global_from_context(SnSymbol name, struct SnContext*)                ATTR_HOT;
CAPI VALUE snow_set_global(SnSymbol name, VALUE val)                                     ATTR_HOT;
CAPI VALUE snow_set_global_from_context(SnSymbol name, VALUE val, struct SnContext*)     ATTR_HOT;
CAPI VALUE snow_get_unspecified_local_from_context(SnSymbol name, struct SnContext*)     ATTR_HOT;
CAPI SnClass* snow_get_class(SnObjectType);
CAPI SnClass** snow_get_basic_types();
CAPI SnObject* snow_get_prototype(SnObjectType);
CAPI const char* snow_value_to_cstr(VALUE val);
CAPI const char* snow_inspect_value(VALUE val);
CAPI bool snow_eval_truth(VALUE val)                                                     ATTR_HOT;
CAPI bool snow_is_normal_object(VALUE val) /* only true for derivates of SnObject */     ATTR_HOT;
CAPI bool snow_prototype_chain_contains(VALUE val, SnObject* proto);
CAPI int snow_compare_objects(VALUE a, VALUE b)                                          ATTR_HOT;
CAPI bool snow_is_object_assigned(VALUE obj)                                             ATTR_HOT;
CAPI VALUE snow_set_object_assigned(VALUE obj, SnSymbol assigned_name, VALUE member_of)  ATTR_HOT;
CAPI VALUE snow_reset_object_assigned(VALUE obj)                                         ATTR_HOT;

CAPI VALUE snow_store_add(VALUE val)                                                     ATTR_HOT;
CAPI VALUE snow_store_get(VALUE key)                                                     ATTR_HOT;

#endif /* end of include guard: SNOW_H_YMYJ9XZE */
