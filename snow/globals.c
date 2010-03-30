#include "snow/globals.h"
#include "snow/intern.h"
#include "snow/snow.h"
#include "snow/exception.h"
#include <stdio.h>

static inline void set_global(SnContext* ctx, SnSymbol name, VALUE val) {
	snow_context_set_local(ctx, name, val);
}

SNOW_FUNC(_puts) {
	for (intx i = 0; i < NUM_ARGS; ++i)
	{
		const char* str = snow_value_to_cstr(ARGS[i]);
		puts(str);
	}
	if (!NUM_ARGS)
		puts("");
	return SN_NIL;
}

SNOW_FUNC(_print) {
	for (intx i = 0; i < NUM_ARGS; ++i)
	{
		const char* str = snow_value_to_cstr(ARGS[i]);
		printf("%s", str);
	}
	return SN_NIL;
}

SNOW_FUNC(_require) {
	REQUIRE_ARGS(1);
	ASSERT_TYPE(ARGS[0], SnStringType);
	SnString* file = (SnString*)ARGS[0];
	return snow_require(snow_string_cstr(file));
}

SNOW_FUNC(_throw) {
	REQUIRE_ARGS(1);
	snow_throw_exception(ARGS[0]);
	return SN_NIL;
}

void snow_init_globals(SnContext* ctx)
{
	// base classes
	for (intx i = SnNormalObjectTypesMin+1; i < SnNormalObjectTypesMax; ++i)
	{
		SnClass* klass = snow_get_class(i);
		ASSERT_TYPE(klass, SnClassType);
		set_global(ctx, klass->name, klass);
	}
	for (intx i = SnThinObjectTypesMin+1; i < SnThinObjectTypesMax; ++i)
	{
		SnClass* klass = snow_get_class(i);
		ASSERT_TYPE(klass, SnClassType);
		set_global(ctx, klass->name, klass);
	}
	for (intx i = SnImmediateTypesMin+1; i < SnImmediateTypesMax; ++i)
	{
		SnClass* klass = snow_get_class(i);
		ASSERT_TYPE(klass, SnClassType);
		set_global(ctx, klass->name, klass);
	}
	
	
	// others
	set_global(ctx, snow_symbol("@"), snow_get_class(SnArrayType));
	set_global(ctx, snow_symbol("LOAD_PATHS"), snow_get_load_paths());
	set_global(ctx, snow_symbol("ARGV"), snow_create_array());
	set_global(ctx, snow_symbol("require"), snow_create_function_with_name(_require, "require"));
	set_global(ctx, snow_symbol("puts"), snow_create_function_with_name(_puts, "puts"));
	set_global(ctx, snow_symbol("print"), snow_create_function_with_name(_print, "print"));
	set_global(ctx, snow_symbol("throw"), snow_create_function_with_name(_throw, "throw"));
}