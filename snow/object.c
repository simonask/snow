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

SnObject* snow_create_object_with_extra_data(SnObject* prototype, uintx extra_bytes, void** extra)
{
	SnObject* obj = (SnObject*)snow_alloc_any_object(SN_OBJECT_TYPE, sizeof(SnObject)+extra_bytes);
	snow_object_init(obj, prototype);
	*extra = (byte*)obj + extra_bytes;
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
	array_init(&obj->included_modules);
}

static inline VALUE object_get_member_without_prototype_but_with_modules(SnObject* obj, VALUE self, SnSymbol member)
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
	
	VALUE from_module = snow_object_get_included_member(obj, self, member);
	if (from_module) return from_module;
	
	return NULL;
}

VALUE snow_object_get_member(SnObject* obj, VALUE self, SnSymbol member)
{
	VALUE val = object_get_member_without_prototype_but_with_modules(obj, self, member);
	if (val) return val;
	
	SnObject* prototype = obj->prototype ? obj->prototype : snow_get_prototype(snow_typeof(obj));
	if (prototype != obj)
	{
		return snow_object_get_member(prototype, self, member);
	}
	
	return NULL;
}

static inline VALUE object_set_with_property(SnObject* obj, VALUE self, SnSymbol member, VALUE val)
{
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
	snow_map_set(obj->members, symbol_to_value(member), val);
	return val;
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

bool snow_object_is_included(SnObject* object, SnObject* included)
{
	if (object == included) return true;
	
	for (uintx i = 0; i < array_size(&object->included_modules); ++i) {
		SnObject* module = (SnObject*)array_get(&object->included_modules, i);
		ASSERT(snow_is_normal_object(module));
		if (snow_object_is_included(module, included)) return true;
	}
	return false;
}

bool snow_object_include(SnObject* obj, SnObject* included)
{
	if (array_find(&obj->included_modules, included) >= 0) return false; // already included
	
	if (snow_object_is_included(included, obj)) snow_throw_exception_with_description("Circular include detected.");
	
	array_push(&obj->included_modules, included);
	return true;
}

bool snow_object_uninclude(SnObject* obj, SnObject* included)
{
	intx idx = array_find(&obj->included_modules, included);
	if (idx >= 0) {
		array_erase(&obj->included_modules, idx);
		return true;
	}
	return false;
}

VALUE snow_object_get_included_member(SnObject* obj, VALUE self, SnSymbol member)
{
	for (uintx i = 0; i < array_size(&obj->included_modules); ++i) {
		SnObject* module = (SnObject*)array_get(&obj->included_modules, i);
		ASSERT(snow_is_normal_object(module));
		VALUE val = object_get_member_without_prototype_but_with_modules(array_get(&obj->included_modules, i), self, member);
		if (val) return val;
	}
	return NULL;
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

SNOW_FUNC(object_to_string) {
	return snow_call_method(SELF, snow_symbol("inspect"), 0);
}

SNOW_FUNC(object_eval) {
	SnArguments* call_arguments = snow_create_arguments_with_size(0);
	
	SnArray* closures = snow_get_member(_context->args, snow_symbol("unnamed"));
	ASSERT_TYPE(closures, SN_ARRAY_TYPE);
	
	VALUE argument = ARG_BY_NAME("argument");
	if (argument)
		snow_arguments_push(call_arguments, argument);
	
	VALUE return_value = SN_NIL;
	for (uintx i = 0; i < snow_array_size(closures); ++i)
		return_value = snow_call_with_args(SELF, snow_array_get(closures, i), call_arguments);
	
	return return_value;
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

SNOW_FUNC(object_prototype) {
	if (snow_is_normal_object(SELF)) {
		SnObject* self = (SnObject*)SELF;
		return self->prototype;
	}
	return snow_get_prototype(snow_typeof(SELF));
}

SNOW_FUNC(object_members) {
	if (snow_is_normal_object(SELF)) {
		SnObject* self = (SnObject*)SELF;
		
		if (!self->members)
			self->members = snow_create_map();
		
		return self->members;
	}
	
	snow_throw_exception_with_description("Tried to access member map of non-normal object.");
	return NULL;
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
	//VALUE member_of = NUM_ARGS > 1 ? ARGS[1] : NULL; // TODO: Use this for something?
	snow_object_set_member(self, self, snow_symbol("__name__"), vname);
	return SELF;
}

SNOW_FUNC(object_is_a) {
	REQUIRE_ARGS(1);
	SnClass* klass = (SnClass*)ARGS[0];
	if (snow_typeof(klass) == SN_CLASS_TYPE) {
		SnObject* proto = klass->instance_prototype;
		if (snow_prototype_chain_contains(SELF, proto)) return SN_TRUE;
	}
	
	if (snow_is_normal_object(SELF) && snow_is_normal_object(ARGS[0]))
	{
		if (snow_prototype_chain_contains(SELF, ARGS[0])) return SN_TRUE;
		if (snow_object_is_included((SnObject*)SELF, (SnObject*)ARGS[0])) return SN_TRUE;
	}
	
	return SN_FALSE;
}

SNOW_FUNC(object_include) {
	REQUIRE_ARGS(1);
	SnObject* module = (SnObject*)ARGS[0];
	ASSERT(snow_is_normal_object(module)); // must include a regular object
	ASSERT(snow_is_normal_object(SELF)); // must include it into a regular object
	SnObject* self = (SnObject*)SELF;
	return boolean_to_value(snow_object_include(self, module));
}

SNOW_FUNC(object_call_member) {
	REQUIRE_ARGS(2);
	ASSERT_TYPE(ARGS[0], SN_SYMBOL_TYPE);
	// FIXME: Pass the Arguments instead.
	return snow_call_method(SELF, value_to_symbol(ARGS[0]), 1, ARGS[1]);
}

SNOW_FUNC(object_property) {
	REQUIRE_ARGS(3);
	
	ASSERT_TYPE(ARGS[0], SN_SYMBOL_TYPE);
	SnSymbol name = value_to_symbol(ARGS[0]);
	
	VALUE getter = ARGS[1];
	VALUE setter = ARGS[2];
	
	if (!is_nil(getter))
		ASSERT_TYPE(getter, SN_FUNCTION_TYPE);
	else
		getter = NULL;
	
	if (!is_nil(setter))
		ASSERT_TYPE(setter, SN_FUNCTION_TYPE);
	else
		setter = NULL;
	
	snow_object_set_property_getter(SELF, name, getter);
	snow_object_set_property_setter(SELF, name, setter);
	
	return SELF;
}

SNOW_FUNC(object_getter) {
	REQUIRE_ARGS(2);
	
	ASSERT_TYPE(ARGS[0], SN_SYMBOL_TYPE);
	SnSymbol name = value_to_symbol(ARGS[0]);
	
	VALUE function = ARGS[1];
	
	if (!is_nil(function))
		ASSERT_TYPE(function, SN_FUNCTION_TYPE);
	else
		function = NULL;
	
	snow_object_set_property_getter(SELF, name, function);
	
	return SELF;
}

SNOW_FUNC(object_setter) {
	REQUIRE_ARGS(2);
	
	ASSERT_TYPE(ARGS[0], SN_SYMBOL_TYPE);
	SnSymbol name = value_to_symbol(ARGS[0]);
	
	VALUE function = ARGS[1];
	
	if (!is_nil(function))
		ASSERT_TYPE(function, SN_FUNCTION_TYPE);
	else
		function = NULL;
	
	snow_object_set_property_setter(SELF, name, function);
	
	return SELF;
}

void init_object_class(SnClass* klass) {
	snow_define_method(klass, "inspect", object_inspect);
	snow_define_method(klass, "to_string", object_to_string);
	snow_define_method(klass, "eval", object_eval);
	snow_define_method(klass, "is_a?", object_is_a);
	snow_define_method(klass, "is_an?", object_is_a);
	snow_define_method(klass, "=", object_equals);
	snow_define_method(klass, "!=", object_not_equals);
	snow_define_property(klass, "name", object_name, NULL);
	snow_define_property(klass, "prototype", object_prototype, NULL);
	snow_define_property(klass, "members", object_members, NULL);
	snow_define_method(klass, "__reset_assigned_name__", object_reset_assigned_name);
	snow_define_method(klass, "__on_assign__", object_on_assign);
	snow_define_method(klass, "include", object_include);
	snow_define_method(klass, "call_member", object_call_member);
	
	snow_define_method(klass, "property", object_property);
	snow_define_method(klass, "getter", object_getter);
	snow_define_method(klass, "setter", object_setter);
}
