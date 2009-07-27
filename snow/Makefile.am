lib_LTLIBRARIES = libsnow.la

if ARCH_x86_64
SOURCES_x86_64 = codegen-x86_64.c asm-x86_64.h continuation-intern-x86_64.c
endif

libsnow_la_SOURCES =  $(SOURCES_x86_64) $(SOURCES_x86_32) \
	arguments.c \
	arguments.h \
	array.c \
	array.h \
	ast.c \
	ast.h \
	basic.h \
	boolean.c \
	class.c \
	class.h \
	codegen-intern.h \
	codegen.c \
	codegen.h \
	context.c \
	context.h \
	continuation.h \
	continuation-intern.h \
	continuation.c \
	function.c \
	function.h \
	gc.c \
	gc.h \
	intern.c \
	intern.h \
	linkbuffer.c \
	linkbuffer.h \
	map.c \
	map.h \
	nil.c \
	numeric.c \
	numeric.h \
	object.c \
	object.h \
	parser.c \
	parser.h \
	scanner.c \
	scanner.h \
	snow.c \
	snow.h \
	str.c \
	str.h \
	symbol.c \
	symbol.h \
	value.h \
	wrapper.c \
	wrapper.h

CLEANFILES = parser.c parser.h scanner.c

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