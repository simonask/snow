#ifndef SYMBOL_H_MKL3HAUK
#define SYMBOL_H_MKL3HAUK

#include "snow/basic.h"
#include "snow/value.h"

typedef uintx SnSymbol;

CAPI SnSymbol snow_symbol(const char* string);
CAPI VALUE snow_vsymbol(const char* string);
CAPI const char* snow_symbol_to_string(SnSymbol sym);

#endif /* end of include guard: SYMBOL_H_MKL3HAUK */
