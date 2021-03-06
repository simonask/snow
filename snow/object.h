#ifndef OBJECT_H_FSS98HM9
#define OBJECT_H_FSS98HM9

#include "snow/basic.h"
#include "snow/symbol.h"

#define SN_NORMAL_OBJECT_TYPE_BASE 0xd0
#define SN_THIN_OBJECT_TYPE_BASE 0xe0
#define SN_IMMEDIATE_TYPE_BASE 0xf0
#define SN_TYPE_MASK 0xf0

typedef enum SnObjectType
{
	SN_OBJECT_TYPE = SN_NORMAL_OBJECT_TYPE_BASE,
	SN_CLASS_TYPE,
//	SN_MODULE_TYPE,
	SN_FUNCTION_TYPE,
	SN_EXCEPTION_TYPE,
	
	SN_NORMAL_OBJECT_TYPE_MAX,
	
	
	SN_CONTINUATION_TYPE = SN_THIN_OBJECT_TYPE_BASE,
	SN_CONTEXT_TYPE,
	SN_ARGUMENTS_TYPE,
	SN_FUNCTION_DESCRIPTION_TYPE,
	SN_STRING_TYPE,
	SN_ARRAY_TYPE,
	SN_MAP_TYPE,
	SN_CODEGEN_TYPE,
	SN_POINTER_TYPE,
	SN_AST_TYPE,
	SN_DEFERRED_TASK_TYPE,
	
	SN_THIN_OBJECT_TYPE_MAX,
	
	
	SN_INTEGER_TYPE = SN_IMMEDIATE_TYPE_BASE,
	SN_NIL_TYPE,
	SN_BOOLEAN_TYPE,
	SN_SYMBOL_TYPE,
	SN_FLOAT_TYPE,

	SN_IMMEDIATE_TYPE_MAX,
	
	SN_TYPE_MAX = SN_IMMEDIATE_TYPE_MAX
} SnObjectType;

typedef struct SnObjectBase
{
	SnObjectType type;
} SnObjectBase;

typedef struct SnObject
{
	SnObjectBase base;
	uint32_t flags;
	struct SnObject* prototype;
	void* members;
	struct array_t property_names;
	struct array_t property_data;
	struct array_t included_modules;
} SnObject;

typedef enum SnObjectFlags
{
	SN_FLAG_ASSIGNED = 1
} SnObjectFlags;

CAPI SnObjectBase* snow_alloc_any_object(SnObjectType type, uintx size);
CAPI SnObject* snow_create_object(SnObject* prototype);
CAPI SnObject* snow_create_object_with_extra_data(SnObject* prototype, uintx extra_bytes, void** out_extra);
CAPI void snow_object_init(SnObject* obj, SnObject* prototype);
CAPI bool snow_object_has_member(SnObject* obj, SnSymbol symbol);
CAPI VALUE snow_object_get_member(SnObject* obj, VALUE self, SnSymbol symbol);
CAPI VALUE snow_object_set_member(SnObject* obj, VALUE self, SnSymbol symbol, VALUE value);
CAPI VALUE snow_object_set_property_getter(SnObject* obj, SnSymbol symbol, VALUE getter);
CAPI VALUE snow_object_set_property_setter(SnObject* obj, SnSymbol symbol, VALUE setter);
CAPI bool snow_object_is_included(SnObject* obj, SnObject* included);
CAPI bool snow_object_include(SnObject* obj, SnObject* included);
CAPI bool snow_object_uninclude(SnObject* obj, SnObject* included);
CAPI VALUE snow_object_get_included_member(SnObject* obj, VALUE self, SnSymbol member);

#endif /* end of include guard: OBJECT_H_FSS98HM9 */
