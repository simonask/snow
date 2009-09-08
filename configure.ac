AC_INIT(snow/main.c)

AM_INIT_AUTOMAKE(snow,0.0.1)

AC_CONFIG_HEADERS(config.h)

AC_CANONICAL_HOST()

AC_PROG_CC([ gcc-mp-4.4 gcc ])

AM_PROG_AS([ gcc-mp-4.4 gcc ])

AM_PROG_LD([ gcc-mp-4.4 gcc ])

AC_PROG_LEX([ flex ])

AC_PROG_YACC([ bison ])

AM_PROG_LIBTOOL
LT_INIT([dlopen,shared])

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

_DEBUG_CFLAGS="-O0 -pg -g -DDEBUG"
_RELEASE_CFLAGS="-Os -pipe -fomit-frame-pointer -Wdisabled-optimization"
_WARNING_CFLAGS="-Wall -Werror=shadow -pedantic -Werror=implicit -Werror=parentheses -Werror=implicit -Winline"

CFLAGS="-std=c99 $ARCH_CFLAGS $_WARNING_CFLAGS $_DEBUG_CFLAGS"
CCASFLAGS="$CFLAGS"
LDFLAGS="-Wl,-all_load -ldl -lpthread"

AC_PROG_LIBTOOL

sLIBS=$LIBS
LIBS=""
AC_SEARCH_LIBS(readline, readline, [], [])
AC_CHECK_FUNCS(readline, [], [])
READLINE_LIBS=$LIBS
LIBS=$sLIBS
AC_SUBST(READLINE_LIBS)

AC_DEFUN([AC_DEFINE_DIR], [
	prefix_NONE=
	exec_prefix_NONE=
	test "x$prefix" = xNONE && prefix_NONE=yes && prefix=$ac_default_prefix
	test "x$exec_prefix" = xNONE && exec_prefix_NONE=yes && exec_prefix=$prefix
dnl In Autoconf 2.60, ${datadir} refers to ${datarootdir}, which in turn
dnl refers to ${prefix}.  Thus we have to use `eval' twice.
	eval ac_define_dir="\"[$]$2\""
	eval ac_define_dir="\"$ac_define_dir\""
	AC_SUBST($1, "$ac_define_dir")
	AC_DEFINE_UNQUOTED($1, "$ac_define_dir", [$3])
	test "$prefix_NONE" && prefix=NONE
	test "$exec_prefix_NONE" && exec_prefix=NONE
])

AC_DEFINE_DIR(SNOW_PATH_PREFIX, prefix, [default working directory])
AC_DEFINE_DIR(SNOW_PATH_LOCALESTATEDIR, localstatedir, [where to put logs etc])
AC_DEFINE_DIR(SNOW_PATH_LIBDIR, libdir, [where to look for plugins])
AC_DEFINE_DIR(SNOW_PATH_SYSCONFDIR, [sysconfdir], [System configuration dir])
AC_DEFINE_DIR(SNOW_PATH_DATADIR, [datadir], [The directory for installing idiosyncratic read-only architecture-independent data.])
AC_DEFINE_DIR(SNOW_PATH_DATAROOTDIR, [datarootdir], [The root of the directory tree for read-only architecture-independent data files.])

AC_OUTPUT(Makefile snow/Makefile lib/Makefile test/Makefile)
