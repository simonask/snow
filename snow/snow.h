#ifndef SNOW_H_YMYJ9XZE
#define SNOW_H_YMYJ9XZE

#include "snow/basic.h"
#include "snow/value.h"
#include "snow/symbol.h"
#include "snow/arguments.h"
#include <stdarg.h>

CAPI void snow_init();
CAPI VALUE snow_eval(const char* str);
CAPI VALUE snow_require(const char* file);

CAPI VALUE snow_call(VALUE self, VALUE closure, uintx num_args, ...);
CAPI VALUE snow_call_va(VALUE self, VALUE closure, uintx num_args, va_list* ap);
CAPI VALUE snow_call_with_args(VALUE self, VALUE closure, SnArguments* args)             ATTR_HOT;
CAPI VALUE snow_call_method(VALUE self, SnSymbol method, uintx num_args, ...)            ATTR_HOT;
CAPI VALUE snow_get_member(VALUE self, SnSymbol member)                                  ATTR_HOT;
CAPI VALUE snow_get_member_by_value(VALUE self, VALUE member)                            ATTR_HOT;
CAPI VALUE snow_set_member(VALUE self, SnSymbol member, VALUE value)                     ATTR_HOT;
CAPI VALUE snow_set_member_by_value(VALUE self, VALUE member, VALUE value)               ATTR_HOT;
CAPI const char* snow_value_to_string(VALUE val);

#endif /* end of include guard: SNOW_H_YMYJ9XZE */
