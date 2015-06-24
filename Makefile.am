AM_CFLAGS = -DFUSE_USE_VERSION=26

## Main build targets
bin_PROGRAMS = ls-fuse
ls_fuse_SOURCES = \
	src/main.c \
	src/ls_fuse.c \
	src/node.c \
	src/parser.c

noinst_HEADERS = \
	src/hash.h \
	src/log.h \
	src/ls_fuse.h \
	src/months.h \
	src/node.h \
	src/parser.h \
	src/tools.h

man_MANS = man/ls-fuse.1

EXTRA_DIST = $(man_MANS) LICENSE README.md packages/ls-fuse.spec
