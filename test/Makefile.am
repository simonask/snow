SUBDIRS = ../snow
bin_PROGRAMS = codegen parser symbol
codegen_SOURCES = codegen.c test.c
codegen_LDADD = ../snow/libsnow.la
parser_SOURCES = parser.c test.c
parser_LDADD = ../snow/libsnow.la
symbol_SOURCES = symbol.c test.c
symbol_LDADD = ../snow/libsnow.la

all: codegen parser symbol

test: all
	exec ./runner.rb codegen parser symbol
