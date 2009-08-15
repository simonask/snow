#include "snow/object.h"
#include "snow/gc.h"
#include "snow/intern.h"
#include "snow/map.h"
#include "snow/snow.h"
#include "snow/str.h"
#include "snow/array-intern.h"

#include <stdio.h>

SnObjectBase* snow_alloc_any_object(SnObjectType type, uintx size)
{
	ASSERT(size > sizeof(SnObjectBase) && "You probably don't want to allocate an SnObjectBase.");
	SnObjectBase* base = (SnObjectBase*)snow_gc_alloc_object(size);
	base->type = type;
	base->flags = 0;
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
	obj->prototype = prototype;
	array_init(&obj->property_names);
	array_init(&obj->property_data);
}

VALUE snow_object_get_member(SnObject* obj, VALUE self, SnSymbol member)
{
	STACK_GUARD;
	
	intx property_idx = array_find(&obj->property_names, symbol_to_value(member));
	if (property_idx >= 0)
	{
		VALUE getter = array_get(&obj->property_data, property_idx<<1);
		if (snow_eval_truth(getter))
			return snow_call(self, getter, 0);
		else
			TRAP(); // write-only property
	}
	
	if (obj->members)
	{
		VALUE val = snow_map_get(obj->members, symbol_to_value(member));
		if (val)
			return val;
	}
	
	SnObject* prototype = obj->prototype ? obj->prototype : snow_get_prototype(typeof(obj));
	if (prototype != obj)
	{
		return snow_object_get_member(prototype, self, member);
	}
	
	return NULL;
}

static inline VALUE object_set_with_property(SnObject* obj, VALUE self, SnSymbol member, VALUE val)
{
	STACK_GUARD;
	
	intx property_idx = array_find(&obj->property_names, symbol_to_value(member));
	if (property_idx >= 0)
	{
		VALUE setter = array_get(&obj->property_data, property_idx<<1|1);
		if (snow_eval_truth(setter))
			return snow_call(self, setter, 1, val);
		else
			TRAP(); // read-only property
	}
	else if (obj->prototype)
	{
		return object_set_with_property(obj->prototype, self, member, val);
	}
	return NULL;
}

VALUE snow_object_set_member(SnObject* obj, VALUE self, SnSymbol member, VALUE val)
{
	if (!obj->members)
		obj->members = snow_create_map();
	
	VALUE with_property = object_set_with_property(obj, self, member, val);
	if (with_property) return with_property;
	return snow_map_set(obj->members, symbol_to_value(member), val);
}

static inline intx create_or_get_index_of_property(SnObject* obj, SnSymbol name)
{
	VALUE vsym = symbol_to_value(name);
	intx idx = array_find(&obj->property_names, vsym);
	if (idx < 0)
	{
		idx = array_size(&obj->property_names);
		array_push(&obj->property_names, vsym);
	}
	return idx;
}

VALUE snow_object_set_property_getter(SnObject* obj, SnSymbol symbol, VALUE getter)
{
	intx idx = create_or_get_index_of_property(obj, symbol);
	array_set(&obj->property_data, idx<<1, getter);
	return getter;
}

VALUE snow_object_set_property_setter(SnObject* obj, SnSymbol symbol, VALUE setter)
{
	intx idx = create_or_get_index_of_property(obj, symbol);
	array_set(&obj->property_data, idx<<1|1, setter);
	return setter;
}

SNOW_FUNC(object_inspect) {
	char cstr[64];
	VALUE vsym_classname = snow_get_member(snow_get_member(SELF, snow_symbol("class")), snow_symbol("name"));
	ASSERT(is_symbol(vsym_classname));
	uint64_t ptr = (uint64_t)SELF;
	snprintf(cstr, 64, "<%s:0x%llx>", snow_symbol_to_cstr(value_to_symbol(vsym_classname)), ptr);
	return snow_create_string(cstr);
}

SNOW_FUNC(object_eval) {
	REQUIRE_ARGS(1);
	snow_call(SELF, ARGS[0], 0);
}

void init_object_class(SnClass* klass)
{
	snow_define_method(klass, "inspect", object_inspect);
	snow_define_method(klass, "object_eval", object_eval);
}