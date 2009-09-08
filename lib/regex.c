#include "snow/snow.h"
#include "snow/library.h"
#include "snow/intern.h"

static void regex_init(SnContext* global_context)
{
	debug("LOL!!!\n");
}

SnLibraryInfo library_info = {
	.name = "Snow Regular Expressions Library",
	.version = 1,
	.initialize = regex_init,
	.finalize = NULL
};
