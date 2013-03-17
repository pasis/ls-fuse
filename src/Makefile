CFLAGS = -Wall -Wunused -Werror

all:
		gcc `pkg-config --cflags fuse` -DFUSE_USE_VERSION=22 $(CFLAGS) main.c `pkg-config --libs fuse`
