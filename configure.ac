AC_INIT([ls-fuse], [dev], [pasis.ua@gmail.com])
AM_INIT_AUTOMAKE([foreign])

AC_CONFIG_FILES([Makefile])

AC_PROG_CC
AM_PROG_CC_C_O

PKG_CHECK_MODULES([fuse], [fuse], [], [AC_MSG_ERROR([fuse is required])])

LIBS="$LIBS $fuse_LIBS"
CFLAGS="$CFLAGS $fuse_CFLAGS"

AC_OUTPUT
