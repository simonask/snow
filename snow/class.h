#ifndef CLASS_H_4Q8LV9FY
#define CLASS_H_4Q8LV9FY

#include "snow/object.h"
#include "snow/str.h"
#include "snow/function.h"

typedef struct SnClass {
	SnObject base;
	SnSymbol name;
	SnObject* instance_prototype;
} SnClass;

CAPI SnClass* snow_create_class(const char* name);
// convenience
CAPI VALUE _snow_define_method(SnClass* klass, const char* name, SnFunctionPtr method, const char* function_name, bool interruptible);
CAPI VALUE _snow_define_class_method(SnClass* klass, const char* name, SnFunctionPtr method, const char* function_name, bool interruptible);
CAPI void _snow_define_property(SnClass* klass, const char* name, SnFunctionPtr getter, const char* getter_name, SnFunctionPtr setter, const char* setter_name);

#define snow_define_method(KLASS, NAME, FUNCTION) _snow_define_method(KLASS, NAME, FUNCTION, #FUNCTION, false)
#define snow_define_method_cc(KLASS, NAME, FUNCTION) _snow_define_method(KLASS, NAME, FUNCTION, #FUNCTION, true)
#define snow_define_class_method(KLASS, NAME, FUNCTION) _snow_define_class_method(KLASS, NAME, FUNCTION, #FUNCTION, false)
#define snow_define_class_method_cc(KLASS, NAME, FUNCTION) _snow_define_class_method(KLASS, NAME, FUNCTION, #FUNCTION, true)
#define snow_define_property(KLASS, NAME, GETTER, SETTER) _snow_define_property(KLASS, NAME, GETTER, #GETTER, SETTER, #SETTER);

#endif /* end of include guard: CLASS_H_4Q8LV9FY */
