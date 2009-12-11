#include "config.h"

#include "snow/snow.h"
#include "snow/object.h"
#include "snow/gc.h"
#include "snow/intern.h"
#include "snow/function.h"
#include "snow/codegen.h"
#include "snow/parser.h"
#include "snow/linkbuffer.h"
#include "snow/str.h"
#include "snow/exception.h"
#include <stdlib.h>

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
	printf(PACKAGE " " VERSION " (prealpha super unstable) [" ARCH_NAME "]\n");
}

static void try_execute_line(void* userdata)
{
	SnLinkBuffer* input_buffer = (SnLinkBuffer*)userdata;
	SnString* str = snow_create_string_from_linkbuffer(input_buffer);
	VALUE result = snow_eval(snow_string_cstr(str));
	printf("=> %s\n", snow_value_to_cstr(snow_call_method(result, snow_symbol("inspect"), 0)));
}

static void catch_display_exception(VALUE exception, void* userdata)
{
	fprintf(stderr, "UNHANDLED EXCEPTION: %s\n\n", snow_value_to_cstr(exception));
}

static void ensure_clear_input_buffer(void* userdata)
{
	SnLinkBuffer* input_buffer = (SnLinkBuffer*)userdata;
	snow_linkbuffer_clear(input_buffer);
}

static void interactive_prompt()
{
	const char* global_prompt = "snow> ";
	const char* unfinished_prompt = "snow*> ";
	bool unfinished_expr = false;
	
	SnLinkBuffer* input_buffer = snow_create_linkbuffer(1024);

	char* line;
	while ((line = readline(unfinished_expr ? unfinished_prompt : global_prompt)) != NULL) {
		if (*line) // strlen(line) != 0
			add_history(line);
		snow_linkbuffer_push_string(input_buffer, line);
		free(line);

		//unfinished_expr = is_expr_unfinished(buffer.str());
		if (!unfinished_expr) {
			snow_try_catch_ensure(try_execute_line, catch_display_exception, ensure_clear_input_buffer, input_buffer);
		}
	}
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
			{"debug",       no_argument,       &debug_mode,       'd'},
			{"version",     no_argument,       NULL,              'v'},
			{"require",     required_argument, NULL,              'r'},
			{"interactive", no_argument,       &interactive_mode,  1 },
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
			case 'i':
			{
				interactive_mode = true;
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
	
	for (uintx i = 0; i < snow_array_size(require_files); ++i) {
		snow_require(snow_string_cstr(((SnString*)snow_array_get(require_files, i))));
	}
	
	if (interactive_mode) {
		interactive_prompt();
	}
	
	return 0;
}