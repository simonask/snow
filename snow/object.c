#include "snow/object.h"
#include "snow/gc.h"
#include "snow/intern.h"
#include "snow/map.h"
#include "snow/snow.h"

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
	snow_object_init(obj, prototype);
	return obj;
}

void snow_object_init(SnObject* obj, SnObject* prototype)
{
	obj->prototype = prototype;
	obj->members = NULL;
	obj->prototype = NULL;
}

VALUE snow_object_get_member(SnObject* obj, SnSymbol member)
{
	ASSERT(obj);
	VALUE val = NULL;
	if (obj->members)
	{
		val = snow_map_get(obj->members, symbol_to_value(member));
		if (val)
			return val;
	}
	
	SnObject* prototype = obj->prototype ? obj->prototype : snow_get_prototype(typeof(obj));
	if (prototype != obj)
	{
		return snow_object_get_member(prototype, member);
	}
	
	return NULL;
}

VALUE snow_object_set_member(SnObject* obj, SnSymbol member, VALUE val)
{
	if (!obj->members)
		obj->members = snow_create_map();
	return snow_map_set(obj->members, symbol_to_value(member), val);
}

void init_object_class(SnClass* klass)
{
	
}