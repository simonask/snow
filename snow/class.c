#include "snow/class.h"
#include "snow/snow.h"

SnClass* snow_create_class(const char* name)
{
	SnClass* kl = (SnClass*)snow_alloc_any_object(SN_CLASS_TYPE, sizeof(SnClass));
	snow_object_init((SnObject*)kl, snow_get_basic_type(SN_CLASS_TYPE));
	kl->name = name;
	kl->instance_prototype = snow_create_object(snow_get_basic_type(SN_OBJECT_TYPE));
	return kl;
}

SnObject* create_class_prototype()
{
	SnObject* proto = snow_create_object(NULL);
	return proto;
}