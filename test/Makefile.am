SUBDIRS = ../snow
bin_PROGRAMS = codegen symbol
codegen_SOURCES = codegen.c test.c
codegen_LDADD = ../snow/libsnow.la
symbol_SOURCES = symbol.c test.c
symbol_LDADD = ../snow/libsnow.la

all: codegen symbol

test: all
	exec ./runner.rb codegen symbol
