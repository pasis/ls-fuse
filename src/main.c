/* main.c
 * ls-fuse - ls -lR output mounter
 *
 * Copyright (C) 2013 Dmitry Podgorny <pasis.ua@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <unistd.h>

#include <fuse.h>
#include <stdio.h>
#include <string.h>

#include "ls_fuse.h"
#include "node.h"
#include "parser.h"
#include "tools.h"
#include "log.h"

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

static void usage(const char * const name)
{
#ifdef PACKAGE_STRING
	printf(PACKAGE_STRING "\n\n");
#endif /* PACKAGE_STRING */
	printf("Usage: %s [FILES ...] [FUSE_OPTIONS] MOUNT_POINT\n", name);
}

int main(int argc, char **argv)
{
	int err = 0;
	int count;

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	/* if --help option passed */
	if ( argc == 2 && ((strcmp(argv[1], "-h") == 0) ||
			   (strcmp(argv[1], "--help") == 0)) ) {
		usage(argv[0]);
		return 0;
	}

	if (parser_init() != 0) {
		return 1;
	}

	count = 0;
	while (argc > 2 && argv[1][0] != '-') {
		/* if parse_file() fails count still will be increased */
		++count;
		err = parse_file(argv[1]);
		if (err != 0) {
			LOGE("Can't process file %s", argv[1]);
			break;
		}
		++argv;
		--argc;
	}

	if (count == 0) {
		err = parse_fd(STDIN_FILENO);
		if (err != 0) {
			LOGE("Can't process <stdin>");
		}
	}

	parser_destroy();

	if (err != 0) {
		/* allocated memory will be freed on exit */
		return 2;
	}

	return fuse_main(argc, argv, &fuse_oper, NULL);
}
