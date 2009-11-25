#include "snow/snow.h"
#include "snow/library.h"
#include "snow/intern.h"
#include "snow/wrapper.h"

#include <regex.h>

typedef struct RegExInternal {
	regex_t regex;
} RegExInternal;

static void regex_free(void* ptr)
{
	RegExInternal* intern = (RegExInternal*)ptr;
	regfree(&intern->regex);
}


SNOW_FUNC(regex_initialize)
{
	RegExInternal* intern = (RegExInternal*)snow_gc_alloc(sizeof(RegExInternal));
	if (regcomp(&intern->regex, "PATTERN", REG_EXTENDED) != 0) {
		snow_throw_exception_with_description("STUB ERROR MESSAGE -- failed to compile regex.");
	}
	
	SnWrapper* pointer = snow_create_wrapper(intern, regex_free);
	snow_set_member(SELF, snow_symbol("_pointer"), pointer);
	return SELF;
}


static void regex_init(SnContext* global_context)
{
	SnClass* RegEx = snow_create_class("RegEx");
	snow_define_method(RegEx, "initialize", regex_initialize);
	
	snow_context_set_local(global_context, snow_symbol("RegEx"), RegEx);
}

SnLibraryInfo library_info = {
	.name = "Snow Regular Expressions Library",
	.version = 1,
	.initialize = regex_init,
	.finalize = NULL
};
