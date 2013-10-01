/* ls_fuse.c
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

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fuse.h>
#include <string.h>

#include "node.h"
#include "ls_fuse.h"
#include "tools.h"

#define SELINUX_XATTR "security.selinux"

static int fuse_getattr(const char *path, struct stat *stbuf)
{
	lsnode_t *node;

	memset(stbuf, 0, sizeof(struct stat));

	node = node_from_path(path);
	if (!node) {	
		return -ENOENT;
	}

	if ((node->mode & S_IFDIR) == S_IFDIR) {
		stbuf->st_nlink = node->ndir + 2;
	} else {
		stbuf->st_nlink = 1;
	}
	stbuf->st_mode = node->mode;
	stbuf->st_size = node->size;
	/* number of 512B blocks allocated */
	stbuf->st_blocks = (node->size + 511) / 512;
	stbuf->st_rdev = node->rdev;
	stbuf->st_uid = node->uid;
	stbuf->st_gid = node->gid;
	stbuf->st_mtime = node->time;

	return 0;
}

static int fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			off_t offset, struct fuse_file_info *fi)
{
	lsnode_t *parent;
	lsnode_t *node;
	char *name;

	(void)offset;
	(void)fi;

	parent = node_from_path(path);
	if (!parent) {
		return -ENOENT;
	}
	if ((parent->mode & S_IFDIR) != S_IFDIR) {
		return -ENOTDIR;
	}

	if (filler(buf, ".", NULL, 0) == 1 ||
	    filler(buf, "..", NULL, 0) == 1) {
		return -EINVAL;
	}

	node = parent->entry;
	while (node) {
		name = node->name;
		if (name != NULL && strcmp(name, ".") != 0 &&
		    strcmp(name, "..") != 0) {
			if (filler(buf, node->name, NULL, 0) == 1) {
				return -EINVAL;
			}
		}
		node = node->next;
	}

	return 0;
}

static int fuse_readlink(const char *path, char *buf, size_t size)
{
	lsnode_t *node;
	size_t len;

	node = node_from_path(path);
	if (!node) {
		return -ENOENT;
	}
	if ((node->mode & S_IFLNK) != S_IFLNK) {
		return -EINVAL;
	}
	if (!node->data) {
		return -EIO;
	}

	len = strlen(node->data);
	if (len >= size) {
		return -EFAULT;
	}

	memcpy(buf, node->data, len);
	buf[len] = '\0';

	return 0;
}

static int fuse_open(const char *path, struct fuse_file_info *fi)
{
	lsnode_t *node;

	node = node_from_path(path);
	if (!node) {
		return -ENOENT;
	}

	if ((fi->flags & O_WRONLY) == O_WRONLY) {
		return -EACCES;
	}

	fi->direct_io = 1;

	if (!node->data) {
		node_create_data(node);
	}

	return 0;
}

static int fuse_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	lsnode_t *node;
	size_t len;
	char *ptr;

	(void)fi;

	node = node_from_path(path);
	if (!node) {
		return -EIO;
	}

	if (!node->data) {
		return -EINVAL;
	}

	len = strlen(node->data);
	if (offset > (off_t)len) {
		return -EFAULT;
	}

	ptr = &node->data[offset];
	len = len - offset;
	if (len > size) {
		len = size;
	}

	memcpy(buf, ptr, len);

	return (int)len;
}

static int fuse_listxattr(const char *path, char *buf, size_t size)
{
	size_t xattr_len = sizeof(SELINUX_XATTR);

	(void)path;

	if (size < xattr_len) {
		return -ERANGE;
	}

	strncpy(buf, SELINUX_XATTR, xattr_len);

	return (int)xattr_len;
}

static int fuse_getxattr(const char *path, const char *name, char *buf,
			 size_t size)
{
	lsnode_t *node;
	size_t len;

	/* only selinux xattr is supported */
	if (strcmp(name, SELINUX_XATTR) != 0) {
		return -ENODATA;
	}

	node = node_from_path(path);
	if (!node) {
		return -ENOENT;
	}

	if (!node->selinux) {
		return -ENODATA;
	}

	len = strlen(node->selinux);
	if (len >= size) {
		return -ERANGE;
	}

	strcpy(buf, node->selinux);

	return len + 1;
}

struct fuse_operations fuse_oper = {
	.getattr = fuse_getattr,
	.readdir = fuse_readdir,
	.readlink = fuse_readlink,
	.open = fuse_open,
	.read = fuse_read,
	.listxattr = fuse_listxattr,
	.getxattr = fuse_getxattr,
};
