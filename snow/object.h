#ifndef OBJECT_H_FSS98HM9
#define OBJECT_H_FSS98HM9

#include "snow/basic.h"
#include "snow/objects.h"

CAPI SnAnyObject* snow_alloc_any_object(SnValueType type);
CAPI SnObject* snow_create_object(SnObject* prototype);
CAPI SnObject* snow_create_object_with_extra_data(SnObject* prototype, uintx extra_bytes, void** out_extra);
CAPI void snow_object_init(SnObject* obj);
CAPI bool snow_object_has_member(SnObject* obj, SnSymbol symbol);
CAPI VALUE snow_object_get_member(SnObject* obj, VALUE self, SnSymbol symbol);
CAPI VALUE snow_object_set_member(SnObject* obj, VALUE self, SnSymbol symbol, VALUE value);
CAPI VALUE snow_object_set_property_getter(SnObject* obj, SnSymbol symbol, VALUE getter);
CAPI VALUE snow_object_set_property_setter(SnObject* obj, SnSymbol symbol, VALUE setter);
CAPI bool snow_object_is_included(SnObject* obj, SnObject* included);
CAPI bool snow_object_include(SnObject* obj, SnObject* included);
CAPI bool snow_object_uninclude(SnObject* obj, SnObject* included);
CAPI VALUE snow_object_get_included_member(SnObject* obj, VALUE self, SnSymbol member);

#endif /* end of include guard: OBJECT_H_FSS98HM9 */
