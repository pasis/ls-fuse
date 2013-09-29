/* node.h
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

#ifndef LS_FUSE_NODE_H
#define LS_FUSE_NODE_H

struct lsnode {
	mode_t mode;
	uid_t uid;
	gid_t gid;
	off_t size;
	dev_t rdev;
	int month;
	time_t time;
	char *selinux;
	char *name;
	char *data;
	/* number of subdirectories */
	int ndir;
	struct lsnode *entry;
	struct lsnode *next;
};

typedef struct lsnode lsnode_t;
typedef void (*handler_t)(lsnode_t *, const char *);

#endif /* LS_FUSE_NODE_H */
