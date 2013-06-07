AC_INIT([ls-fuse], [0.2dev], [pasis.ua@gmail.com])
AM_INIT_AUTOMAKE([foreign])

AC_CONFIG_FILES([Makefile])

AC_PROG_CC
AM_PROG_CC_C_O

AC_ARG_ENABLE([debug], [AS_HELP_STRING([--enable-debug], [enable debug output])])

PKG_CHECK_MODULES([fuse], [fuse], [], [AC_MSG_ERROR([fuse is required])])

LIBS="$LIBS $fuse_LIBS"
CFLAGS="$CFLAGS $fuse_CFLAGS"

if test "x$enable_debug" = xyes; then
	CFLAGS="$CFLAGS -DDEBUG"
else
	CFLAGS="$CFLAGS -DNDEBUG"
fi

AC_OUTPUT
