#include "snow/class.h"
#include "snow/snow.h"
#include "snow/symbol.h"
#include "snow/function.h"
#include "snow/intern.h"


void snow_init_class_class(SnClass** class_class)
{
	SnClass* kl = (SnClass*)snow_alloc_any_object(SN_CLASS_TYPE, sizeof(SnClass));
	snow_object_init((SnObject*)kl, NULL);
	kl->name = snow_symbol("Class");
	kl->instance_prototype = snow_create_object(NULL);
	kl->base.prototype = kl->instance_prototype; // Class is the prototype of Class
	snow_set_member(kl->instance_prototype, snow_symbol("class"), kl);
	*class_class = kl;
}

SnClass* snow_create_class(const char* name)
{
	SnClass* kl = (SnClass*)snow_alloc_any_object(SN_CLASS_TYPE, sizeof(SnClass));
	snow_object_init((SnObject*)kl, snow_get_prototype(SN_CLASS_TYPE));
	kl->name = snow_symbol(name);
	kl->instance_prototype = snow_create_object(NULL); // NULL => Object is prototype
	snow_set_member(kl->instance_prototype, snow_symbol("class"), kl);
	return kl;
}

VALUE _snow_define_method(SnClass* klass, const char* name, SnFunctionPtr function, const char* function_name)
{
	ASSERT_TYPE(klass, SN_CLASS_TYPE);
	ASSERT(klass->instance_prototype);
	ASSERT(name);
	ASSERT(function);
	SnSymbol sym = snow_symbol(name);
	SnFunction* func = snow_create_function_with_name(function, function_name);
	// TODO: mark as leaf scope
	snow_set_member(klass->instance_prototype, sym, func);
	return func;
}

VALUE _snow_define_class_method(SnClass* klass, const char* name, SnFunctionPtr function, const char* function_name)
{
	ASSERT_TYPE(klass, SN_CLASS_TYPE);
	ASSERT(name);
	ASSERT(function);
	SnSymbol sym = snow_symbol(name);
	SnFunction* func = snow_create_function_with_name(function, function_name);
	snow_set_member(klass, sym, func);
	return func;
}

void _snow_define_property(SnClass* klass, const char* name, SnFunctionPtr getter, const char* getter_name, SnFunctionPtr setter, const char* setter_name)
{
	ASSERT_TYPE(klass, SN_CLASS_TYPE);
	ASSERT(name);
	SnSymbol sym = snow_symbol(name);
	// XXX: Property functions are always uninterruptible. Consider this, please.
	if (getter)
	{
		SnFunction* getter_func = snow_create_function_with_name(getter, getter_name);
		snow_object_set_property_getter(klass->instance_prototype, sym, getter_func);
	}
	if (setter)
	{
		SnFunction* setter_func = snow_create_function_with_name(setter, setter_name);
		snow_object_set_property_setter(klass->instance_prototype, sym, setter_func);
	}
}

SNOW_FUNC(class_new) {
	SnClass* self = SELF;
	ASSERT_TYPE(self, SN_CLASS_TYPE);
	ASSERT(self->instance_prototype);
	SnObject* new_object = snow_create_object(self->instance_prototype);
	VALUE initialize = snow_object_get_member(new_object, new_object, snow_symbol("initialize"));
	if (initialize)
	{
		snow_call_with_args(new_object, initialize, _context->args);
	}
	return new_object;
}

SNOW_FUNC(class_get_name) {
	ASSERT_TYPE(SELF, SN_CLASS_TYPE);
	SnClass* self = SELF;
	return symbol_to_value(self->name);
}

SNOW_FUNC(class_set_name) {
	ASSERT_TYPE(SELF, SN_CLASS_TYPE);
	REQUIRE_ARGS(1);
	SnClass* self = SELF;
	ASSERT_TYPE(ARGS[0], SN_SYMBOL_TYPE);
	self->name = value_to_symbol(ARGS[0]);
	return ARGS[0];
}

SNOW_FUNC(class_new_class) {
	SnClass* self = (SnClass*)SELF;
	ASSERT_TYPE(self, SN_CLASS_TYPE);
	SnClass* new_class = snow_create_class("<Anonymous Class>");
	
	if (NUM_ARGS > 0)
	{
		// inheritance
		ASSERT(is_object(ARGS[0]));
		ASSERT_TYPE(ARGS[0], SN_CLASS_TYPE); // TODO: exception
		new_class->instance_prototype->prototype = ((SnClass*)ARGS[0])->instance_prototype;
	}
	
	return new_class;
}

SNOW_FUNC(class_instance_prototype) {
	SnClass* self = (SnClass*)SELF;
	ASSERT_TYPE(self, SN_CLASS_TYPE);
	return self->instance_prototype;
}

void init_class_class(SnClass* klass)
{
	snow_define_class_method(klass, "__call__", class_new_class);
	snow_define_method(klass, "__call__", class_new);
	snow_define_property(klass, "__name__", class_get_name, class_set_name);
	snow_define_property(klass, "instance_prototype", class_instance_prototype, NULL);
}
