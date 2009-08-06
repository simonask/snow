#include "snow/debug.h"
#include "snow/intern.h"
#include "snow/continuation.h"
#include "snow/context.h"
#include "snow/symbol.h"

static const char* symstr(SnSymbol sym)
{
	return snow_symbol_to_string(sym);
}

void snow_debug_print_return_chain()
{
	SnContinuation* cc = snow_get_current_continuation();
	int level = 0;
	while (cc) {
		debug("#%d\t(%s)\tcc: 0x%llx\tfunction: 0x%llx\tcontext: 0x%llx\n", level++, (cc->context && cc->context->function) ? symstr(cc->context->function->desc->name) : "", cc, cc->function, cc->context);
		cc = cc->return_to;
	}
}