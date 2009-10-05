#ifndef LIBRARY_H_8PJPZXWC
#define LIBRARY_H_8PJPZXWC

#include "snow/snow.h"

typedef void(*SnLibraryInitializeFunc)(struct SnContext* global_context);
typedef void(*SnLibraryFinalizeFunc)();

typedef struct SnLibraryInfo {
	const char* name;
	uintx version;
	SnLibraryInitializeFunc initialize;
	SnLibraryFinalizeFunc finalize;
} SnLibraryInfo;

/*
	In your external dynamic library, declare library info like so:
	
	SnLibraryInfo library_info {
		.name = "My Library",
		.version = 123,
		.initialize = my_library_init,
		.finalize = my_library_finalize
	};
	
	Snow will look for the library_info symbol, and call your initializer when
	the library is loaded.
*/

#endif /* end of include guard: LIBRARY_H_8PJPZXWC */
