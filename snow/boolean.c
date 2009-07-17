#include "snow/object.h"

SnObject* create_boolean_prototype()
{
	SnObject* proto = snow_create_object(NULL);
	return proto;
}