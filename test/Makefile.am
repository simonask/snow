test_names = $(shell ls *.c | sed -e s/\.c//)

compile:
	CC="$(CC)" CFLAGS="$(CFLAGS) ../snow/.libs/libsnow.a -I.." ./compile.rb $(test_names)

all: compile

test: compile
	./runner.rb $(test_names)
	
clean:
	rm -f $(test_names)
	rm -rf *.dSYM