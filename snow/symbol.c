#include "snow/symbol.h"
#include "snow/intern.h"
#include "snow/snow.h"
#include "snow/str.h"
#include "snow/array.h"
#include <string.h>

static VALUE symbol_storage_key = NULL;

static SnArray* symbol_storage() {
	if (!symbol_storage_key)
		symbol_storage_key = snow_store_add(snow_create_array_with_size(1024));
	return (SnArray*)snow_store_get(symbol_storage_key);
}

SnSymbol snow_symbol(const char* cstr)
{
	// FIXME: Mutex lock
	
	SnArray* storage = symbol_storage();
	ASSERT(storage);
	ASSERT_TYPE(storage, SN_ARRAY_TYPE);
	
	SnString* str = snow_create_string(cstr);
	
	for (uintx i = 0; i < snow_array_size(storage); ++i) {
		SnString* existing = (SnString*)snow_array_get(storage, i);
		ASSERT_TYPE(existing, SN_STRING_TYPE);
		
		if (snow_string_compare(str, existing) == 0) {
			return i;
		}
	}
	
	SnSymbol new_symbol = snow_array_size(storage);
	snow_array_push(storage, str);
	return new_symbol;
}

SnSymbol snow_symbol_from_string(SnString* str)
{
	ASSERT_TYPE(str, SN_STRING_TYPE);
	return snow_symbol(str->str);
}

VALUE snow_vsymbol(const char* cstr)
{
	return symbol_to_value(snow_symbol(cstr));
}

const char* snow_symbol_to_string(SnSymbol sym)
{
	SnArray* storage = symbol_storage();
	ASSERT(storage);
	ASSERT(sym < snow_array_size(storage));
	SnString* str = snow_array_get(storage, sym);
	ASSERT(str != SN_NIL);
	return str->str;
}

void init_symbol_class(SnClass* klass)
{
	
}