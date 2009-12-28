#include "snow/object.h"
#include "snow/gc.h"
#include "snow/intern.h"
#include "snow/map.h"
#include "snow/snow.h"
#include "snow/str.h"
#include "snow/array-intern.h"
#include "snow/lock.h"

#include <stdio.h>

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
	obj->flags = SN_FLAG_ASSIGNED; // new objects are "assigned" by default, so they will only get a name assigned if asked for by calling object.__reset_assigned_name__().
	obj->prototype = prototype;
	obj->members = NULL;
	obj->prototype = prototype;
	array_init(&obj->property_names);
	array_init(&obj->property_data);
}

VALUE snow_object_get_member(SnObject* obj, VALUE self, SnSymbol member)
{
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
	
	SnObject* prototype = obj->prototype ? obj->prototype : snow_get_prototype(snow_typeof(obj));
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
	VALUE name = snow_get_member(SELF, snow_symbol("__name__"));
	const char* name_to_string = name ? snow_value_to_cstr(name) : NULL;
	snprintf(cstr, 64, "<%s%s%s@0x%llx>", name_to_string ? name_to_string : "", name_to_string ? ":" : "", snow_symbol_to_cstr(value_to_symbol(vsym_classname)), ptr);
	return snow_create_string(cstr);
}

SNOW_FUNC(object_eval) {
	REQUIRE_ARGS(1);
	return snow_call(SELF, ARGS[0], 0);
}

SNOW_FUNC(object_equals) {
	REQUIRE_ARGS(1);
	return boolean_to_value(SELF == ARGS[0]);
}

SNOW_FUNC(object_not_equals) {
	REQUIRE_ARGS(1);
	return boolean_to_value(SELF != ARGS[0]);
}

SNOW_FUNC(object_name) {
	return snow_get_member(SELF, snow_symbol("__name__"));
}

SNOW_FUNC(object_reset_assigned_name) {
	/*
		This function resets the SN_FLAG_ASSIGNED flag on the object,
		causing it to get a new name the next time it is assigned to a named member or local.
	*/
	
	ASSERT(snow_is_normal_object(SELF));
	SnObject* self = (SnObject*)SELF;
	self->flags &= ~SN_FLAG_ASSIGNED;
	return SELF;
}

SNOW_FUNC(object_on_assign) {
	ASSERT(snow_is_normal_object(SELF));
	SnObject* self = (SnObject*)SELF;
	VALUE vname = ARGS[0];
	ASSERT(is_symbol(vname));
	VALUE member_of = NUM_ARGS > 1 ? ARGS[1] : NULL; // TODO: Use this for something?
	snow_object_set_member(self, self, snow_symbol("__name__"), vname);
	return SELF;
}

SNOW_FUNC(object_is_a) {
	REQUIRE_ARGS(1);
	SnClass* klass = (SnClass*)ARGS[0];
	ASSERT_TYPE(klass, SN_CLASS_TYPE); // must specify a class object
	SnObject* proto = klass->instance_prototype;
	return boolean_to_value(snow_prototype_chain_contains(SELF, proto));
}

void init_object_class(SnClass* klass)
{
	snow_define_method(klass, "inspect", object_inspect);
	snow_define_method(klass, "to_string", object_inspect);
	snow_define_method(klass, "object_eval", object_eval);
	snow_define_method(klass, "is_a?", object_is_a);
	snow_define_method(klass, "is_an?", object_is_a);
	snow_define_method(klass, "=", object_equals);
	snow_define_method(klass, "!=", object_not_equals);
	snow_define_property(klass, "name", object_name, NULL);
	snow_define_method(klass, "__reset_assigned_name__", object_reset_assigned_name);
	snow_define_method(klass, "__on_assign__", object_on_assign);
}