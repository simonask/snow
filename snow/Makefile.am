lib_LTLIBRARIES = libsnow.la

if ARCH_x86_64
SOURCES_ARCH = arch-x86_64.c codegen-x86_64.c asm-x86_64.h continuation-intern-x86_64.c
HEADERS_ARCH = arch-x86_64.h
endif

if PARALLEL_IS_DISPATCH
SOURCES_PARALLEL = task-dispatch.c
else
if PARALLEL_IS_OPENMP
SOURCES_PARALLEL = task-openmp.c
else
SOURCES_PARALLEL = task-serial.c
endif
endif

libsnow_la_SOURCES = $(SOURCES_ARCH) $(SOURCES_PARALLEL) \
	arguments.c \
	array.c \
	ast.c \
	boolean.c \
	class.c \
	codegen.c \
	context.c \
	continuation.c \
	debug.c \
	exception.c \
	fixed-alloc.c \
	function.c \
	gc.c \
	globals.c \
	intern.c \
	linkbuffer.c \
	map.c \
	nil.c \
	numeric.c \
	object.c \
	parser.c \
	scanner.c \
	snow.c \
	str.c \
	symbol.c \
	task.c \
	pointer.c


include_snowdir = $(includedir)/snow
include_snow_HEADERS = $(HEADERS_ARCH) \
	arch.h \
	arguments.h \
	array.h \
	ast.h \
	basic.h \
	class.h \
	classes.h \
	codegen.h \
	context.h \
	continuation.h \
	function.h \
	gc.h \
	globals.h \
	library.h \
	linkbuffer.h \
	lock.h \
	map.h \
	numeric.h \
	object.h \
	parser.h \
	scanner.h \
	str.h \
	symbol.h \
	task.h \
	pointer.h

include_HEADERS = snow.h
	
noinst_HEADERS = \
	array-intern.h \
	codegen-intern.h \
	continuation-intern.h \
	fixed-alloc.h \
	debug.h \
	task-intern.h \
	intern.h



CLEANFILES = parser.c parser.tab.h scanner.c

parser.c parser.tab.h: parser.yy
	$(YACC) --defines=parser.tab.h -o parser.c parser.yy

scanner.c: scanner.ll parser.tab.h
	$(LEX) -d -o scanner.c scanner.ll

libsnow_la_LDFLAGS = -lreadline

INCLUDES = -I@top_srcdir@

bin_PROGRAMS = snow
snow_SOURCES = main.c
snow_LDADD = libsnow.la
snow_LDFLAGS = -static

test: libsnow.la