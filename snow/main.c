#include "config.h"

#include "snow/snow.h"
#include "snow/object.h"
#include "snow/gc.h"
#include "snow/intern.h"
#include "snow/function.h"
#include "snow/codegen.h"
#include "snow/parser.h"

#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <getopt.h>

#if defined(ARCH_x86_64)
#define ARCH_NAME "x86-64"
#else
#define ARCH_NAME "<UNKNOWN ARCH>"
#endif

static void print_version_info()
{
	printf("snow 0.0.0 (prealpha super unstable) [" ARCH_NAME "]\n");
}

static void interactive_prompt()
{
}

int main(int argc, char* const* argv)
{
	snow_init();
	
	static int debug_mode = false;
	static int verbose_mode = false;
	static int interactive_mode = false;
	
	SnArray* require_files = snow_create_array();
	
	while (true)
	{
		int c;
		
		static struct option long_options[] = {
			{"debug",       
			no_argument,     
			  &debug_mode,   
			    'd'},
			{"version",     no_argument,       NULL,              'v'},
			{"interactive", no_argument,       &interactive_mode, 'i'},
			{"require",     required_argument, NULL,              'r'},
			{"verbose",     no_argument,       &verbose_mode,      1 },
			{0,0,0,0}
		};
		
		int option_index = -1;
		
		c = getopt_long(argc, argv, "dvir:", long_options, &option_index);
		if (c < 0)
			break;
		
		switch (c)
		{
			case 'v':
			{
				print_version_info();
				return 0;
			}
			case 'r':
			{
				SnString* filename = snow_create_string(optarg);
				snow_array_push(require_files, filename);
				break;
			}
			case '?':
				TRAP(); // unknown argument
			default:
				break;
		}
	}
	
	if (optind < argc) {
		// require loose filenames
		SnString* filename = snow_create_string(argv[optind++]);
		snow_array_push(require_files, filename);
	}
	
	for (uintx i = 0; i < require_files->size; ++i) {
		snow_require(((SnString*)require_files->data[i])->str);
	}
	
	if (interactive_mode) {
		interactive_prompt();
	}
	
	return 0;
}