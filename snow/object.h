#ifndef OBJECT_H_FSS98HM9
#define OBJECT_H_FSS98HM9

#include "snow/basic.h"
#include "snow/value.h"
#include "snow/symbol.h"

typedef enum SnObjectType
{
	SN_OBJECT_TYPE,
	SN_STRING_TYPE,
	SN_ARRAY_TYPE,
	SN_MAP_TYPE,
	SN_SNOW_FUNCTION_TYPE,
	SN_NATIVE_FUNCTION_TYPE,
	SN_PROPERTY_MAP,
	SN_WRAPPER_TYPE,
	SN_CODEGEN_TYPE,
	SN_AST_TYPE,
} SnObjectType;

typedef struct SnObjectBase
{
	SnObjectType type;
} SnObjectBase;

typedef struct SnObject
{
	union {
		SnObjectBase base;
		SnObjectType type;
	};
	
	struct SnObject* prototype;
	void* members;
	void* properties;
} SnObject;

CAPI SnObjectBase* snow_alloc_any_object(SnObjectType type, uintx size);
CAPI SnObject* snow_create_object(SnObject* prototype);
CAPI bool snow_object_has_member(SnObject* obj, SnSymbol symbol);
CAPI VALUE snow_object_get_member(SnObject* obj, SnSymbol symbol);
CAPI VALUE snow_object_set_member(SnObject* obj, SnSymbol symbol, VALUE value);

#endif /* end of include guard: OBJECT_H_FSS98HM9 */
