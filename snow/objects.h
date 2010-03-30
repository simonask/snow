#pragma once
#ifndef OBJECTS_H_DDXWJ2A4
#define OBJECTS_H_DDXWJ2A4

#include "snow/basic.h"

typedef enum SnObjectFlags
{
	SN_FLAG_ASSIGNED = 1
} SnObjectFlags;

#define SN_NORMAL_OBJECT_TYPE_BASE 0x1000
#define SN_THIN_OBJECT_TYPE_BASE 0x2000
#define SN_IMMEDIATE_TYPE_BASE 0x3000
#define SN_TYPE_MASK 0xf000

typedef enum SnValueType
{
	#define SN_BEGIN_THIN_CLASS(NAME) NAME ## Type,
	#define SN_BEGIN_CLASS(NAME) NAME ## Type,
	#define SN_BEGIN_THIN_CLASSES() SnThinObjectTypesMin = SN_THIN_OBJECT_TYPE_BASE,
	#define SN_BEGIN_CLASSES() SnThinObjectTypesMax, SnNormalObjectTypesMin = SN_NORMAL_OBJECT_TYPE_BASE,
	#define SN_END_CLASSES() SnNormalObjectTypesMax,
	#include "snow/objects-decl.h"
	
	SnImmediateTypesMin = SN_IMMEDIATE_TYPE_BASE,
	SnIntegerType,
	SnNilType,
	SnBooleanType,
	SnSymbolType,
	SnFloatType,
	SnImmediateTypesMax,
	SnValueTypeMax = SnImmediateTypesMax
} SnValueType;

// typedefs/predeclarations
#define SN_BEGIN_THIN_CLASS(NAME) struct NAME; typedef struct NAME NAME;
#define SN_BEGIN_CLASS(NAME) struct NAME; typedef struct NAME NAME;
#include "snow/objects-decl.h"

#define SN_BEGIN_THIN_CLASS(NAME) struct NAME { SnValueType type;
#define SN_BEGIN_CLASS(NAME) struct NAME { struct SnObject base;
#define SN_END_CLASS() };
#define SN_MEMBER_DATA(TYPE, NAME) TYPE NAME;
#define SN_MEMBER_ROOT(TYPE, NAME) TYPE NAME;
#define SN_MEMBER_ROOT_ARRAY(TYPE, NAME, SIZE_MEMBER) TYPE NAME;
#include "snow/objects-decl.h"

typedef union {
	SnValueType type;
	
	#define SN_BEGIN_THIN_CLASS(NAME) struct NAME as_ ## NAME;
	#define SN_BEGIN_CLASS(NAME) struct NAME as_ ## NAME;
	#include "snow/objects-decl.h"
} SnAnyObject;

#define SNOW_CAST_OBJECT(OBJECT, TARGET_TYPE) (&(OBJECT)->as_ ## TARGET_TYPE)

static inline SnValueType snow_typeof(VALUE val)
{
	if (snow_is_object(val)) return ((SnAnyObject*)val)->type;
	if (snow_is_integer(val)) return SnIntegerType;
	if (snow_is_nil(val) || !val) return SnNilType;
	if (snow_is_boolean(val)) return SnBooleanType;
	if (snow_is_symbol(val)) return SnSymbolType;
	if (snow_is_float(val)) return SnFloatType;
	return -1;
}

#endif /* end of include guard: OBJECTS_H_DDXWJ2A4 */
