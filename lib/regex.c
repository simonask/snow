#include "snow/snow.h"
#include "snow/library.h"
#include "snow/intern.h"
#include "snow/pointer.h"

#include <regex.h>

typedef struct RegExInternal {
	regex_t regex;
} RegExInternal;

static void regex_free(void* ptr)
{
	RegExInternal* intern = (RegExInternal*)ptr;
	regfree(&intern->regex);
	snow_free(intern);
}


SNOW_FUNC(regex_initialize)
{
	RegExInternal* intern = (RegExInternal*)snow_malloc(sizeof(RegExInternal));
	if (regcomp(&intern->regex, "PATTERN", REG_EXTENDED) != 0) {
		snow_throw_exception_with_description("STUB ERROR MESSAGE -- failed to compile regex.");
	}
	
	SnPointer* pointer = snow_create_pointer(intern, regex_free);
	snow_set_member(SELF, snow_symbol("_pointer"), pointer);
	return SELF;
}


static void regex_init(SnContext* global_context)
{
	SnClass* RegEx = snow_create_class("RegEx", NULL);
	snow_define_method(RegEx, "initialize", regex_initialize);
	
	snow_set_global(snow_symbol("RegEx"), RegEx);
}

SnLibraryInfo library_info = {
	.name = "Snow Regular Expressions Library",
	.version = 1,
	.initialize = regex_init,
	.finalize = NULL
};
