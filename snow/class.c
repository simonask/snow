#include "snow/class.h"

SnObject* create_class_prototype()
{
	SnObject* proto = snow_create_object(NULL);
	return proto;
}