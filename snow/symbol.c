#include "snow/symbol.h"
#include "snow/intern.h"
#include "snow/snow.h"
#include "snow/str.h"
#include "snow/array.h"
#include "snow/lock.h"
#include <string.h>

static VALUE symbol_storage_key = NULL;
static SnLock symbol_table_lock;
static bool symbol_table_lock_init = false;

static SnArray* symbol_storage() {
	if (!symbol_storage_key)
		symbol_storage_key = snow_store_add(snow_create_array_with_size(1024));
	return (SnArray*)snow_store_get(symbol_storage_key);
}

SnSymbol snow_symbol(const char* cstr)
{
	if (!symbol_table_lock_init)
	{
		snow_init_lock(&symbol_table_lock);
		symbol_table_lock_init = true;
	}
	
	snow_lock(&symbol_table_lock);
	
	SnArray* storage = symbol_storage();
	ASSERT(storage);
	ASSERT_TYPE(storage, SN_ARRAY_TYPE);
	
	SnSymbol retval;
	
	for (uintx i = 0; i < snow_array_size(storage); ++i) {
		SnString* existing = (SnString*)snow_array_get(storage, i);
		ASSERT_TYPE(existing, SN_STRING_TYPE);
		
		if (strcmp(cstr, snow_string_cstr(existing)) == 0) {
			retval = i;
			goto out;
		}
	}
	
	SnString* str = snow_create_string(cstr);
	retval = snow_array_size(storage);
	snow_array_push(storage, str);
	
	out:
	snow_unlock(&symbol_table_lock);
	return retval;
}

SnSymbol snow_symbol_from_string(SnString* str)
{
	ASSERT_TYPE(str, SN_STRING_TYPE);
	return snow_symbol(snow_string_cstr(str));
}

VALUE snow_vsymbol(const char* cstr)
{
	return symbol_to_value(snow_symbol(cstr));
}

const char* snow_symbol_to_cstr(SnSymbol sym)
{
	return snow_string_cstr(snow_symbol_to_string(sym));
}

SnString* snow_symbol_to_string(SnSymbol sym)
{
	snow_lock(&symbol_table_lock);
	SnArray* storage = symbol_storage();
	ASSERT(storage);
	ASSERT(sym < snow_array_size(storage));
	SnString* str = snow_array_get(storage, sym);
	ASSERT(str);
	snow_unlock(&symbol_table_lock);
	return str;
}

SNOW_FUNC(symbol___call__) {
	ASSERT(is_symbol(SELF));
	REQUIRE_ARGS(1);
	
	return snow_call_method(ARGS[0], value_to_symbol(SELF), 0);
}

SNOW_FUNC(symbol_to_string) {
	ASSERT(is_symbol(SELF));
	return snow_symbol_to_string(value_to_symbol(SELF));
}

SNOW_FUNC(symbol_inspect) {
	ASSERT(is_symbol(SELF));
	return snow_string_concatenate(snow_create_string("#"), snow_symbol_to_string(value_to_symbol(SELF)));
}

void init_symbol_class(SnClass* klass)
{
	snow_define_method(klass, "__call__", symbol___call__);
	snow_define_method(klass, "to_string", symbol_to_string);
	snow_define_method(klass, "inspect", symbol_inspect);
}
