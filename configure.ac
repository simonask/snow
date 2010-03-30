AC_INIT(snow/main.c)

AM_INIT_AUTOMAKE(snow, 0.0.1)

AC_CONFIG_HEADERS(config.h)

AC_CANONICAL_HOST()

AC_PROG_CC([clang gcc])
AM_PROG_AS([clang gcc])
AM_PROG_LD([ld])
AC_PROG_LEX([flex])
AC_PROG_YACC([bison])

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

AC_CHECK_HEADER(dispatch/dispatch.h,
	[
		if test $CC = "clang"; then
			# Only use GCD if clang exists
			PARALLEL_FRAMEWORK="dispatch"
			AC_DEFINE([PARALLEL_IS_DISPATCH], [1], [Whether or not the parallel framework in use is Grand Central Dispatch.])
		else
			echo "WARNING: Grand Central Dispatch found, but not using Clang. Disabling parallelism."
		fi
	],
	[
		echo "Parallelism disabled (parallel operations will run in serial)."
	]
)

AM_CONDITIONAL(PARALLEL_IS_DISPATCH, test "x$PARALLEL_FRAMEWORK" = xdispatch)
AM_CONDITIONAL(PARALLEL_IS_DISABLED, test "x$PARALLEL_FRAMEWORK" = x)

AC_CHECK_HEADER(dtrace.h,
	[
		AC_DEFINE([DTRACE], [1], [Compile Snow with DTrace hooks.])
	],
	[
		echo "DTrace not found, disabling instrumentation."
	]
)

GCC_DEBUG_CFLAGS="-O0 -g -pg -DDEBUG"
GCC_RELEASE_CFLAGS="-Os -pipe -Wdisabled-optimization"
GCC_WARNING_CFLAGS="-pedantic-errors -Wall -Werror -Wno-unused"
GCC_CFLAGS="-std=c99 $ARCH_CFLAGS $GCC_WARNING_CFLAGS $GCC_DEBUG_CFLAGS"

CLANG_DEBUG_CFLAGS="-O0 -g -DDEBUG"
CLANG_RELEASE_CFLAGS="-Os -Wdisabled-optimization"
CLANG_PROFILE_CFLAGS="-Os -g -Wdisabled-optimization"
CLANG_WARNING_CFLAGS="-Wall -Werror -Wno-error=unused-value"
CLANG_CFLAGS="-std=c99 -fblocks $PARALLEL_CFLAGS $ARCH_CFLAGS $CLANG_WARNING_CFLAGS $CLANG_DEBUG_CFLAGS"

case $CC in
	clang*)
		echo "Using Clang compiler."
		CFLAGS="$CLANG_CFLAGS"
	;;
	gcc*)
		echo "Using GCC compiler."
		CFLAGS="$GCC_CFLAGS"
	;;
esac

CCASFLAGS="$CFLAGS"
LDFLAGS="$PARALLEL_LDFLAGS -Wl,-all_load -ldl -lm"

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

AC_DEFINE([SNOW_VERSION_MAJOR], [0], [Major version component])
AC_DEFINE([SNOW_VERSION_MINOR], [1], [Minor version component])
AC_DEFINE([SNOW_VERSION_BUILD], [0], [Build version component])
AC_DEFINE([SNOW_VERSION_PATCH], [0], [Patch version component])

AC_OUTPUT(Makefile snow/Makefile lib/Makefile test/Makefile)
