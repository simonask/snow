#ifndef SYMBOL_H_MKL3HAUK
#define SYMBOL_H_MKL3HAUK

#include "snow/basic.h"

typedef uintx SnSymbol;

struct SnString;

CAPI SnSymbol snow_symbol(const char* string);
CAPI SnSymbol snow_symbol_from_string(struct SnString* str);
CAPI VALUE snow_vsymbol(const char* string);
CAPI const char* snow_symbol_to_string(SnSymbol sym);

#endif /* end of include guard: SYMBOL_H_MKL3HAUK */
