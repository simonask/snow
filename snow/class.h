#ifndef CLASS_H_4Q8LV9FY
#define CLASS_H_4Q8LV9FY

#include "snow/object.h"
#include "snow/str.h"
#include "snow/function.h"
#include "snow/gc.h"

typedef struct SnClass {
	SnObject base;
	SnSymbol name;
	SnObject* instance_prototype;
} SnClass;

CAPI void snow_init_class_class(SnClass** class_class);
CAPI SnClass* snow_create_class(const char* name);

// convenience
CAPI VALUE _snow_define_method(SnClass* klass, const char* name, SnFunctionPtr method, const char* function_name);
CAPI VALUE _snow_define_class_method(SnClass* klass, const char* name, SnFunctionPtr method, const char* function_name);
CAPI void _snow_define_property(SnClass* klass, const char* name, SnFunctionPtr getter, const char* getter_name, SnFunctionPtr setter, const char* setter_name);

#define snow_define_method(KLASS, NAME, FUNCTION) _snow_define_method(KLASS, NAME, FUNCTION, #FUNCTION)
#define snow_define_class_method(KLASS, NAME, FUNCTION) _snow_define_class_method(KLASS, NAME, FUNCTION, #FUNCTION)
#define snow_define_property(KLASS, NAME, GETTER, SETTER) _snow_define_property(KLASS, NAME, GETTER, #GETTER, SETTER, #SETTER);

// data wrapping
typedef void(*SnDataInitFunc)(void* data);
typedef void(*SnDataFinalizeFunc)(void* data);

typedef struct SnWrapClass {
	SnClass base;
	const char* struct_name;
	size_t struct_size;
	
	SnDataInitFunc init_func;
	SnDataFinalizeFunc free_func;
} SnWrapClass;

CAPI SnClass* _snow_create_class_wrap_struct(const char* name, const char* struct_name, size_t struct_size, SnDataInitFunc init_func, SnDataFinalizeFunc free_func);
#define snow_create_class_wrap_struct(NAME, STRUCT, INIT_FUNC, FREE_FUNC) _snow_create_class_wrap_struct(NAME, #STRUCT, sizeof(STRUCT), (SnDataInitFunc)INIT_FUNC, (SnDataFinalizeFunc)FREE_FUNC)

CAPI SnObject* snow_create_wrap_object(SnClass* wrap_class);

CAPI void* _snow_unwrap_struct(SnObject* obj, uintx struct_size, const char* struct_name);
#define snow_unwrap_struct(OBJ, STRUCT) (STRUCT*)_snow_unwrap_struct(OBJ, sizeof(STRUCT), #STRUCT);

#endif /* end of include guard: CLASS_H_4Q8LV9FY */
