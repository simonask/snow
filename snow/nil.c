#include "snow/object.h"

SnObject* create_nil_prototype()
{
	SnObject* proto = snow_create_object(NULL);
	return proto;
}