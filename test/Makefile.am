SUBDIRS = ../snow
noinst_PROGRAMS = arch codegen exception parallel parser symbol
arch_SOURCES = arch.c test.c
arch_LDADD = ../snow/libsnow.la
arch_LDFLAGS = -static
codegen_SOURCES = codegen.c test.c
codegen_LDADD = ../snow/libsnow.la
codegen_LDFLAGS = -static
exception_SOURCES = exception.c test.c
exception_LDADD = ../snow/libsnow.la
exception_LDFLAGS = -static
parallel_SOURCES = parallel.c test.c
parallel_LDADD = ../snow/libsnow.la
parallel_LDFLAGS = -static
parser_SOURCES = parser.c test.c
parser_LDADD = ../snow/libsnow.la
parser_LDFLAGS = -static
symbol_SOURCES = symbol.c test.c
symbol_LDADD = ../snow/libsnow.la
symbol_LDFLAGS = -static

all: arch codegen exception parallel parser symbol

test: all
	exec ./runner.rb arch codegen exception parallel parser symbol
