#include "snow/object.h"
#include "snow/gc.h"
#include "snow/intern.h"
#include "snow/map.h"

SnObjectBase* snow_alloc_any_object(SnObjectType type, uintx size)
{
	ASSERT(size > sizeof(SnObjectBase) && "You probably don't want to allocate an SnObjectBase.");
	SnObjectBase* base = (SnObjectBase*)snow_gc_alloc_object(size);
	base->type = type;
	return base;
}

SnObject* snow_create_object(SnObject* prototype)
{
	SnObject* obj = (SnObject*)snow_alloc_any_object(SN_OBJECT_TYPE, sizeof(SnObject));
	obj->prototype = prototype;
	obj->members = snow_create_map();
	obj->properties = NULL;
	return obj;
}

VALUE snow_object_get_member(SnObject* obj, SnSymbol member)
{
	VALUE val = snow_map_get(obj->members, symbol_to_value(member));
	ASSERT(val);
	return val;
}

VALUE snow_object_set_member(SnObject* obj, SnSymbol member, VALUE val)
{
	return snow_map_set(obj->members, symbol_to_value(member), val);
}

SnObject* create_object_prototype()
{
	SnObject* proto = snow_create_object(NULL);
	return proto;
}