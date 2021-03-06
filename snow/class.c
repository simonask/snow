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
	_snow_define_object_property(klass->instance_prototype, name, getter, getter_name, setter, setter_name);
}

void _snow_define_object_property(SnObject* object, const char* name, SnFunctionPtr getter, const char* getter_name, SnFunctionPtr setter, const char* setter_name)
{
	ASSERT(name);
	SnSymbol sym = snow_symbol(name);
	// XXX: Property functions are always uninterruptible. Consider this, please.
	if (getter)
	{
		SnFunction* getter_func = snow_create_function_with_name(getter, getter_name);
		snow_object_set_property_getter(object, sym, getter_func);
	}
	if (setter)
	{
		SnFunction* setter_func = snow_create_function_with_name(setter, setter_name);
		snow_object_set_property_setter(object, sym, setter_func);
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
	
	SnFunction *yield = NULL;
	SnClass* superclass = NULL;
	
	if (NUM_ARGS == 0);
	else if (NUM_ARGS == 1) {
		if (snow_typeof(ARGS[0]) == SN_FUNCTION_TYPE)
			yield = ARGS[0];
		else if (snow_typeof(ARGS[0]) == SN_CLASS_TYPE)
			superclass = ARGS[0];
		else
			TRAP(); // single argument must be class or function
	} else {
		yield = ARGS[1];
		superclass = ARGS[0];
	}
	
	if (superclass) {
		ASSERT(is_object(superclass));
		ASSERT_TYPE(superclass, SN_CLASS_TYPE);
		new_class->instance_prototype->prototype = ((SnClass*)ARGS[0])->instance_prototype;
	}
	
	if (yield) {
		ASSERT(is_object(yield));
		ASSERT_TYPE(yield, SN_FUNCTION_TYPE);
		snow_call(new_class->instance_prototype, yield, 0);
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

static void class_wrap_instance_free(VALUE o) {
	SnObject* obj = (SnObject*)o;
	ASSERT(snow_gc_allocated_size(obj) > sizeof(SnObject));
	byte* data = (byte*)o;
	SnGCFreeFunc free_func = *(SnGCFreeFunc*)(data + sizeof(SnObject));
	data += sizeof(SnObject) + sizeof(SnGCFreeFunc);
	if (free_func) free_func(data);
}

SNOW_FUNC(class_wrap_new) {
	SnObject* new_object = snow_create_wrap_object((SnClass*)SELF);
	
	VALUE initialize = snow_object_get_member(new_object, new_object, snow_symbol("initialize"));
	if (initialize)
	{
		snow_call_with_args(new_object, initialize, _context->args);
	}
	
	return new_object;
}

SnClass* _snow_create_class_wrap_struct(const char* name, const char* struct_name, size_t struct_size, SnDataInitFunc init_func, SnDataFinalizeFunc free_func)
{
	SnWrapClass* kl = (SnWrapClass*)snow_alloc_any_object(SN_CLASS_TYPE, sizeof(SnWrapClass));
	snow_object_init((SnObject*)kl, snow_get_prototype(SN_CLASS_TYPE));
	kl->base.name = snow_symbol(name);
	kl->base.instance_prototype = snow_create_object(NULL); // NULL => Object is prototype
	snow_set_member(kl->base.instance_prototype, snow_symbol("class"), kl);
	kl->struct_name = struct_name;
	kl->struct_size = struct_size;
	kl->init_func = init_func;
	kl->free_func = free_func;
	snow_define_class_method(&kl->base, "__call__", class_wrap_new);
	return &kl->base;
}

SnObject* snow_create_wrap_object(SnClass* wrap_class)
{
	ASSERT_TYPE(wrap_class, SN_CLASS_TYPE);
	ASSERT(snow_gc_allocated_size(wrap_class) == snow_gc_round(sizeof(SnWrapClass))); // hack to try to make sure that we're not in fact a regular class.
	SnWrapClass* self = (SnWrapClass*)wrap_class;
	ASSERT(self->base.instance_prototype);
	
	/*
		Layout of wrapped structs:
		
		{
			SnObject base;
			SnGCFreeFunc free_func; // identical to the free_func in the class
			byte extra[...];        // size of the wrapped struct
		}
	*/
	
	void* extra;
	SnObject* new_object = snow_create_object_with_extra_data(self->base.instance_prototype, self->struct_size + sizeof(SnGCFreeFunc), &extra);
	SnGCFreeFunc* free_func = (SnGCFreeFunc*)extra;
	*free_func = self->free_func;
	extra = (byte*)extra + sizeof(SnGCFreeFunc);
	
	memset(extra, 0, self->struct_size);
	
	if (self->init_func) self->init_func(extra);
	snow_gc_set_free_func(new_object, class_wrap_instance_free);
	
	return new_object;
}

void* _snow_unwrap_struct(SnObject* obj, uintx struct_size, const char* struct_name)
{
	ASSERT(snow_gc_allocated_size(obj) == snow_gc_round(sizeof(SnObject) + sizeof(SnGCFreeFunc) + struct_size));
	return ((byte*)obj) + sizeof(SnObject) + sizeof(SnGCFreeFunc);
}