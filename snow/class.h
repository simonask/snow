#ifndef CLASS_H_4Q8LV9FY
#define CLASS_H_4Q8LV9FY

#include "snow/object.h"
#include "snow/str.h"
#include "snow/function.h"

typedef struct SnClass {
	SnObject base;
	SnString* name;
	SnObject* instance_prototype;
} SnClass;

CAPI SnClass* snow_create_class(const char* name);
CAPI VALUE snow_define_method(SnClass* klass, const char* name, SnFunctionPtr method);	// convenience

#endif /* end of include guard: CLASS_H_4Q8LV9FY */
