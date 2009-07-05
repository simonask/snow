AC_INIT(snow/main.c)

AM_INIT_AUTOMAKE(snow-c,0.1)

AC_CONFIG_HEADERS(config.h)

AC_CANONICAL_HOST()

AC_PROG_CC([ gcc-mp-4.4 ])

AM_PROG_AS([ gcc-mp-4.4 ])

AC_PROG_LEX([ flex ])

AC_PROG_YACC([ bison ])

ARCH="unknown"

case $host_cpu in
	i[[3456]]86*)
		echo "Building for x86-32."
		ARCH="x86_32"
		ARCH_CFLAGS="-DARCH_x86_32";;
	x86_64*)
		echo "Building for x86-64."
		ARCH="x86_64"
		ARCH_CFLAGS="-m64 -DARCH_x86_64";;
esac

AM_CONDITIONAL(ARCH_x86_64, test "x$ARCH" = xx86_64)
AM_CONDITIONAL(ARCH_x86_32, test "x$ARCH" = xx86_32)

__DEBUG_CFLAGS="-fopenmp -O0 -pg -g -DDEBUG -Wall"
__RELEASE_CFLAGS="-fopenmp -Os -pipe -fomit-frame-pointer"
CFLAGS="$__DEBUG_CFLAGS $ARCH_CFLAGS"
CCASFLAGS="$CFLAGS"
LDFLAGS="-ldl"

AC_PROG_LIBTOOL

sLIBS=$LIBS
LIBS=""
AC_SEARCH_LIBS(readline, readline, [], [])
AC_CHECK_FUNCS(readline, [], [])
READLINE_LIBS=$LIBS
LIBS=$sLIBS
AC_SUBST(READLINE_LIBS)


AC_OUTPUT(Makefile snow/Makefile)
