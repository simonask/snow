#ifndef OBJECT_H_FSS98HM9
#define OBJECT_H_FSS98HM9

#include "snow/basic.h"
#include "snow/value.h"
#include "snow/symbol.h"

typedef enum SnObjectType
{
	SN_OBJECT_TYPE,
	SN_CLASS_TYPE,
	SN_CONTINUATION_TYPE,
	SN_CONTEXT_TYPE,
	SN_ARGUMENTS_TYPE,
	SN_FUNCTION_TYPE,
	SN_FUNCTION_DESCRIPTION_TYPE,
	SN_STRING_TYPE,
	SN_ARRAY_TYPE,
	SN_MAP_TYPE,
	SN_WRAPPER_TYPE,
	SN_CODEGEN_TYPE,
	SN_AST_TYPE,
	
	// immediates (pseudo-immediates on 32-bit)
	SN_INTEGER_TYPE,
	SN_NIL_TYPE,
	SN_BOOLEAN_TYPE,
	SN_SYMBOL_TYPE,
	SN_FLOAT_TYPE,
	
	SN_NUMBER_OF_BASIC_TYPES
} SnObjectType;

typedef struct SnObjectBase
{
	SnObjectType type;
} SnObjectBase;

typedef struct SnObject
{
	SnObjectBase base;
	
	struct SnObject* prototype;
	void* members;
	void* properties;
} SnObject;

CAPI SnObjectBase* snow_alloc_any_object(SnObjectType type, uintx size);
CAPI SnObject* snow_create_object(SnObject* prototype);
CAPI void snow_object_init(SnObject* obj, SnObject* prototype);
CAPI bool snow_object_has_member(SnObject* obj, SnSymbol symbol);
CAPI VALUE snow_object_get_member(SnObject* obj, SnSymbol symbol);
CAPI VALUE snow_object_set_member(SnObject* obj, SnSymbol symbol, VALUE value);

#endif /* end of include guard: OBJECT_H_FSS98HM9 */
