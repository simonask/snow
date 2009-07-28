#include "snow/symbol.h"
#include "snow/intern.h"
#include <string.h>

typedef struct Symbol {
	const char* str;
} Symbol;

static Symbol* symbol_table = NULL;
static uintx symbol_table_size = 0;

SnSymbol snow_symbol(const char* str)
{
	uintx i;
	for (i = 0; i < symbol_table_size; ++i) {
		if (strcmp(str, symbol_table[i].str) == 0) {
			return i;
		}
	}
	
	// TODO: preallocate
	symbol_table = (Symbol*)realloc(symbol_table, sizeof(Symbol)*(symbol_table_size+1));
	i = symbol_table_size++;
	symbol_table[i].str = str;
	
	debug("Adding symbol: \"%s\" (%d)\n", str, i);
	return i;
}

static const char* symbol_to_string(SnSymbol sym)
{
	ASSERT(sym < symbol_table_size);
	return symbol_table[sym].str;
}

SnObject* create_symbol_prototype()
{
	SnObject* proto = snow_create_object(NULL);
	return proto;
}