#include "snow/globals.h"
#include "snow/intern.h"
#include "snow/snow.h"

static inline void set_global(SnContext* ctx, const char* name, VALUE val) {
	snow_context_set_local(ctx, snow_symbol(name), val);
}

void snow_init_globals(SnContext* ctx)
{
	set_global(ctx, "@", snow_get_class(SN_ARRAY_TYPE));
}