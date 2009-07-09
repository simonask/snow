lib_LTLIBRARIES = libsnow.la

if ARCH_x86_64
SOURCES_x86_64 = codegen-x86_64.c asm-x86_64.h
endif

libsnow_la_SOURCES =  $(SOURCES_x86_64) $(SOURCES_x86_32) \
	array.c \
	array.h \
	ast.c \
	ast.h \
	basic.h \
	codegen.h \
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
	object.c \
	object.h \
	parser.c \
	parser.h \
	scanner.c \
	scanner.h \
	snow.c \
	snow.h \
	string.c \
	string.h \
	symbol.c \
	symbol.h \
	value.h

CLEANFILES = parser.c parser.h

parser.c parser.h: parser.yy
	$(YACC) --defines=parser.tab.h -o parser.c parser.yy

scanner.c: scanner.ll parser.h
	$(LEX) -d -o scanner.c scanner.ll

libsnow_la_LDFLAGS = -lreadline

INCLUDES = -I@top_srcdir@

bin_PROGRAMS = snow
snow_SOURCES = main.c
snow_LDADD = libsnow.la
snow_LDFLAGS = -static