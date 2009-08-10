#include "snow/globals.h"
#include "snow/intern.h"
#include "snow/snow.h"

static inline void set_global(SnContext* ctx, SnSymbol name, VALUE val) {
	snow_context_set_local(ctx, name, val);
}

void snow_init_globals(SnContext* ctx)
{
	// base classes
	for (intx i = SN_NORMAL_OBJECT_TYPE_BASE; i < SN_NORMAL_OBJECT_TYPE_MAX; ++i)
	{
		SnClass* klass = snow_get_class(i);
		ASSERT_TYPE(klass, SN_CLASS_TYPE);
		set_global(ctx, klass->name, klass);
	}
	for (intx i = SN_THIN_OBJECT_TYPE_BASE; i < SN_THIN_OBJECT_TYPE_MAX; ++i)
	{
		SnClass* klass = snow_get_class(i);
		ASSERT_TYPE(klass, SN_CLASS_TYPE);
		set_global(ctx, klass->name, klass);
	}
	for (intx i = SN_IMMEDIATE_TYPE_BASE; i < SN_IMMEDIATE_TYPE_MAX; ++i)
	{
		SnClass* klass = snow_get_class(i);
		ASSERT_TYPE(klass, SN_CLASS_TYPE);
		set_global(ctx, klass->name, klass);
	}
	
	
	// others
	set_global(ctx, snow_symbol("@"), snow_get_class(SN_ARRAY_TYPE));
	
	set_global(ctx, snow_symbol("LOAD_PATHS"), snow_get_load_paths());
}