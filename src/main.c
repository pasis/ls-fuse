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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>

#include <assert.h>
#include <errno.h>
#include <fuse.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "months.h"
#include "hash.h"

#define ERR 1
#define OK 0

#ifndef FALSE
#define FALSE 0
#endif /* FALSE */
#ifndef TRUE
#define TRUE 1
#endif /* TRUE */

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define MAX_READ_BUFSIZ (1024 * 1024)
#define STR_BUFSIZ 4096

/* maximum number of regex matches */
#define MATCH_NUM 10
/* parts of regexp */
#define R_SPACE "[ \t]+"
#define R_SPACE_OPT "[ \t]*"
#define R_NUM "[0-9]+"
#define R_NUM_OPT "[0-9]*"
#define R_TYPE "([-bcdlps])"
#define R_MODE "([-rwxsS]{9,9})"
#define R_XMODE "[t@+.]?"
#define R_USR "([0-9A-Za-z_-]+)"
#define R_GRP "([0-9A-Za-z_-]+)"
#define R_SIZ "([0-9]+|[0-9]+,[ \t]+[0-9]+)"
#define R_MONTH "([^ \t]+)"
#define R_DATE "([1-3]?[0-9][ \t]+[0-9]{4,4}|[1-3]?[0-9][ \t]+[0-2]?[0-9]:[0-5][0-9])"
#define R_SELINUX "([0-9a-zA-Z:_.-]+)"
#define R_NAME "(.+)"
#define R_BS R_SPACE_OPT R_NUM_OPT R_SPACE_OPT

#define SELINUX_XATTR "security.selinux"

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
	int ndir;             /* number of subdirectories */
	struct lsnode *entry;
	struct lsnode *next;
};

typedef struct lsnode lsnode_t;
typedef void (*handler_t)(lsnode_t *, const char *);

static void node_set_type(lsnode_t *, const char *);
static void node_set_mode(lsnode_t *, const char *);
static void node_set_usr(lsnode_t *, const char *);
static void node_set_grp(lsnode_t *, const char *);
static void node_set_size(lsnode_t *, const char *);
static void node_set_month(lsnode_t *, const char *);
static void node_set_time(lsnode_t *, const char *);
static void node_set_selinux(lsnode_t *, const char *);
static void node_set_name(lsnode_t *, const char *);

static lsnode_t root = {
	.mode = S_IFDIR | 0755,
	.name = "/",
};
static lsnode_t *cwd = &root;

static hash_tbl_t hash_usr;
static hash_tbl_t hash_grp;

/* lsreg - regex for ls -l and ls -lR */
/* 1 - file type
 * 2 - file mode (rwx)
 * 3 - owner
 * 4 - group
 * 5 - size or major, minor
 * 6 - month
 * 7 - day and time or day and year
 * 8 - file name
 */
static regex_t lsreg;
static char lsreg_str[] =
	"^" R_BS R_TYPE R_MODE R_XMODE R_SPACE R_NUM R_SPACE R_USR R_SPACE
	R_GRP R_SPACE R_SIZ R_SPACE R_MONTH R_SPACE R_DATE R_SPACE R_NAME "$";
static handler_t lsreg_tbl[MATCH_NUM] = {NULL, node_set_type, node_set_mode,
	node_set_usr, node_set_grp, node_set_size, node_set_month,
	node_set_time, node_set_name,};

/* lsregx - regex for ls -lZ and ls -lRZ */
/* 1 - file type
 * 2 - file mode (rwx)
 * 3 - owner
 * 4 - group
 * 5 - selinux context
 * 6 - file name
 */
static regex_t lsregx;
static char lsregx_str[] =
	"^" R_TYPE R_MODE R_XMODE R_SPACE R_USR R_SPACE R_GRP R_SPACE R_SELINUX
	R_SPACE R_NAME "$";
static handler_t lsregx_tbl[MATCH_NUM] = {NULL, node_set_type, node_set_mode,
	node_set_usr, node_set_grp, node_set_selinux, node_set_name,};

char *str_ptr;
size_t str_len;
int str_idx;

static lsnode_t *node_alloc(void)
{
	lsnode_t *node = malloc(sizeof(lsnode_t));
	if (!node) {
		return NULL;
	}

	memset(node, 0, sizeof(lsnode_t));
	node->next = cwd->entry;
	cwd->entry = node;

	return node;
}

/*
static void node_free(lsnode_t *node)
{
	free(node);
}
*/

/* TODO: functions node_set_xxx must be refactored to support multiple files.
 *       now pointers are just set without checking, but they can be set before
 *       while previous file parsing - this will cause memory leaks.
 */

static void node_set_type(lsnode_t *node, const char * const type)
{
	static struct {
		char key;
		int value;
	} type_map[] = {
		{'-', S_IFREG},
		{'b', S_IFBLK},
		{'c', S_IFCHR},
		{'d', S_IFDIR},
		{'l', S_IFLNK},
		{'p', S_IFIFO},
		{'s', S_IFSOCK},
	};

	int i;
	int s_if;
	char c;

	assert(type != NULL);

	if (strlen(type) != 1) {
		/* wrong string format */
		return;
	}

	s_if = 0;
	c = type[0];
	for (i = 0; i < ARRAY_SIZE(type_map); i++) {
		if (type_map[i].key == c) {
			s_if = type_map[i].value;
			break;
		}
	}

	/* TODO: check if file type is set and whether it equals to s_if
	 *       (for multiple files support)*/
	if (s_if) {
		node->mode |= s_if;
		if ((s_if & S_IFDIR) == S_IFDIR) {
			cwd->ndir++;
		}
	}
}

static void node_set_mode(lsnode_t *node, const char * const mode)
{
	mode_t st_mode = 0;

	assert(mode != NULL);

	if (strlen(mode) != 9) {
		/* wrong string format */
		return;
	}

	if (mode[0] == 'r') {
		st_mode |= S_IRUSR;
	}
	if (mode[1] == 'w') {
		st_mode |= S_IWUSR;
	}
	if (mode[2] == 'x') {
		st_mode |= S_IXUSR;
	} else if (mode[2] == 's') {
		st_mode |= S_IXUSR;
		st_mode |= S_ISUID;
	}

	if (mode[3] == 'r') {
		st_mode |= S_IRGRP;
	}
	if (mode[4] == 'w') {
		st_mode |= S_IWGRP;
	}
	if (mode[5] == 'x') {
		st_mode |= S_IXGRP;
	} else if (mode[5] == 's') {
		st_mode |= S_IXGRP;
		st_mode |= S_ISGID;
	}

	if (mode[6] == 'r') {
		st_mode |= S_IROTH;
	}
	if (mode[7] == 'w') {
		st_mode |= S_IWOTH;
	}
	if (mode[8] == 'x') {
		st_mode |= S_IXOTH;
	}

	node->mode |= st_mode;
}

static void node_set_usr(lsnode_t *node, const char * const owner)
{
	struct passwd *pwd;
	char *endptr;
	long uid;

	assert(owner != NULL);

	uid = hash_get(hash_usr, owner);
	if (uid != -1) {
		node->uid = (uid_t)uid;
	} else {
		pwd = getpwnam(owner);
		if (pwd) {
			node->uid = pwd->pw_uid;
		} else {
			/* if owner is numeric */
			uid = strtol(owner, &endptr, 10);
			assert(endptr != NULL);
			if (*endptr == '\0') {
				node->uid = (uid_t)uid;
			}
		}
		hash_add(hash_usr, owner, (long)node->uid);
	}
}

static void node_set_grp(lsnode_t *node, const char * const group)
{
	struct group *grp;
	char *endptr;
	long gid;

	assert(group != NULL);

	gid = hash_get(hash_grp, group);
	if (gid != -1) {
		node->gid = (gid_t)gid;
	} else {
		grp = getgrnam(group);
		if (grp) {
			node->gid = grp->gr_gid;
		} else {
			/* if group is numeric */
			gid = strtol(group, &endptr, 10);
			assert(endptr != NULL);
			if (*endptr == '\0') {
				node->gid = (gid_t)gid;
			}
		}
		hash_add(hash_grp, group, (long)node->gid);
	}
}

static void node_set_size(lsnode_t *node, const char * const size)
{
	char *endptr = NULL;
	long st_size;

	assert(size != NULL);

	st_size = strtol(size, &endptr, 10);
	assert(endptr != NULL);

	if (*endptr == '\0') {
		node->size = st_size;
	} else if (*endptr == ',') {
		/* assume this is major, minor */
		/* TODO: handle incorrect strings */
		if (st_size < (1 << 8)) {
			node->rdev = st_size << 8;
			st_size = strtol(endptr + 1, NULL, 10);
			if (st_size < (1 << 8)) {
				node->rdev |= st_size;
			} else {
				node->rdev = 0;
			}
		}
	}
}

static void node_set_month(lsnode_t *node, const char * const month)
{
	int i;

	assert(month != NULL);

	/* see months.h for month_tbl */
	for (i = 0; i < ARRAY_SIZE(month_tbl); i++) {
		if (strcmp(month, month_tbl[i].key) == 0) {
			node->month = month_tbl[i].val;
			break;
		}
	}
}

static void node_set_time(lsnode_t *node, const char * const time2)
{
	struct tm t;
	time_t unix_time;
	char *tmp_time;
	char *tmp_part;
	size_t len;
	int year;

	assert(time2 != NULL);

	if (time(&unix_time) == (time_t)-1) {
		return;
	}

	if (localtime_r(&unix_time, &t) == NULL) {
		return;
	}

	tmp_time = strdup(time2);
	tmp_part = strtok(tmp_time, " ");
	if (!tmp_part) {
		goto out;
	}

	t.tm_mday = atoi(tmp_part);
	tmp_part = strtok(NULL, " ");
	if (!tmp_part) {
		goto out;
	}

	len = strlen(tmp_part);
	if (len == 5 && tmp_part[2] == ':') {
		tmp_part[2] = '\0';
		t.tm_hour = atoi(tmp_part);
		t.tm_min = atoi(&tmp_part[3]);
	} else if (len == 4) {
		year = atoi(tmp_part);
		if (year >= 1970) {
			t.tm_year = year - 1900;
		} else {
			goto out;
		}
	} else {
		goto out;
	}

	/* assume month is set before */
	t.tm_mon = node->month;
	unix_time = mktime(&t);
	if (unix_time >= 0) {
		node->time = unix_time;
	}

out:
	free(tmp_time);
}

static void node_set_selinux(lsnode_t *node, const char * const ctx)
{
	assert(ctx != NULL);

	node->selinux = strdup(ctx);
}

static void node_set_name(lsnode_t *node, const char * const name)
{
	char *sub;
	char *tmp;
	size_t len;

	assert(name != NULL);

	if ((node->mode & S_IFLNK) == S_IFLNK) {
		#define LNK_DELIM " -> "
		sub = strstr(name, LNK_DELIM);
		if (sub) {
			len = sub - name;
			tmp = (char *)malloc(len + 1);
			memcpy(tmp, name, len);
			tmp[len] = '\0';
			node->name = tmp;
			tmp = sub + strlen(LNK_DELIM);
			if (*tmp != '\0') {
				node->data = strdup(tmp);
			}
		} else {
			node->name = strdup(name);
		}
	} else {
		node->name = strdup(name);
	}
}

/* node_create_data must be thread safe */
static void node_create_data(lsnode_t *node)
{
	static char data[] = "File: %s\n"
			     "Size: %s\n"
			     "Mode: %s\n"
			     "Owner: %s\n"
			     "SELinux context: %s\n";
	static char units[] = {'K', 'M', 'G', 'T', 'P'};

	char *mode;
	char *owner;
	char *selinux;
	char size[8];
	size_t n;
	char sfx = 0;
	int i;

	if (!node->name) {
		return;
	}

	/* TODO: implement mode and owner */
	selinux = owner = mode = "";
	if (node->selinux != NULL) {
		selinux = node->selinux;
	}
	
	n = node->size;
	i = 0;
	while (n >= 10000 && i < ARRAY_SIZE(units)) {
		n /= 1024;
		sfx = units[i];
		i++;
	}
	snprintf(size, sizeof(size), "%d%c", (int)n, sfx);

	n = strlen(node->name) + strlen(size) + strlen(mode) + strlen(owner) +
	    strlen(selinux) + sizeof(data) - 10;

	node->data = malloc(n);
	if (node->data != NULL) {
		snprintf(node->data, n, data, node->name, size, mode, owner,
			 selinux);
	}
}

/* node_from_path must be thread safe */
static lsnode_t *node_from_path(const char * const path)
{
	lsnode_t *parent;
	lsnode_t *node;
	char *tmp = strdup(path);
	char *tok;
	char *saveptr = NULL;
	int found;

	parent = &root;
	tok = strtok_r(tmp, "/", &saveptr);

	while (tok) {
		if (strcmp(tok, ".") == 0) {
			/* do nothing, parent remains the same */
		} else if (strcmp(tok, "..") == 0) {
			/* TODO: not implemented yet (doubly linked list?) */
		} else {
			node = parent->entry;
			found = FALSE;
			while (node) {
				if (node->name && !strcmp(tok, node->name)) {
					found = TRUE;
					parent = node;
					break;
				}
				node = node->next;
			}

			if (!found) {
				parent = NULL;
				break;
			}
		}

		tok = strtok_r(NULL, "/", &saveptr);
	}

	free(tmp);

	return parent;
}

static int parse_line(const char *s, const regex_t *reg, const handler_t h_tbl[])
{
	regmatch_t match[MATCH_NUM];
	int i;
	size_t len = strlen(s);
	size_t sub_len;
	char tmp[len + 1];
	lsnode_t *node;

#ifdef DEBUG
	printf("parsing: %s\n", s); /* debug */
#endif /* DEBUG */

	if (regexec(reg, s, MATCH_NUM, match, 0) == REG_NOMATCH ) {
		return ERR;
	}

	/* TODO: search for node at first (for multiple files support) */
	node = node_alloc();
	if (!node) {
		return ERR;
	}

	for (i = 1; i < MATCH_NUM; i++) {
		if (match[i].rm_so >= 0 && match[i].rm_eo >= match[i].rm_so) {
			/* TODO: after debug removing make tmp string only
			 *       when h_tbl[i] != NULL
			 */
			sub_len = match[i].rm_eo - match[i].rm_so;
			if (sub_len > len) {
				continue;
			}
			strncpy(tmp, &s[match[i].rm_so], sub_len);
			tmp[sub_len] = '\0';

#ifdef DEBUG
			printf("%d: %s\n", i, tmp); /* debug */
#endif /* DEBUG */

			if (h_tbl[i] != NULL) {
				h_tbl[i](node, tmp);
			}
		}
	}

	return OK;
}

static int is_dir(const char * const s)
{
	size_t len;

	assert(s != NULL);

	len = strlen(s);
	if (len < 2) {
		return FALSE;
	}

	if (s[len - 1] == ':') {
		return TRUE;
	}

	return FALSE;
}

static int chcwd(const char * const path)
{
	lsnode_t *node;

	node = node_from_path(path);
	if (!node) {
		/* TODO: create all nodes with standard values for a dir */
		return ERR;
	}

	cwd = node;
	return OK;
}

static int parse(char *line)
{
	size_t len;
	int err;

	err = parse_line(line, &lsreg, lsreg_tbl);
	if (err != OK) {
		err = parse_line(line, &lsregx, lsregx_tbl);
	}

	if (err != OK && is_dir(line)) {
		/* remove last ':' */
		len = strlen(line);
		line[len - 1] = '\0';
		chcwd(line);
	}

	return OK;
}

static int buf_to_str(const char *buf, int start, int end)
{
	size_t len;
	void *tmp_ptr;

	assert(start <= end);
	len = end - start;
	if (str_len - str_idx <= len) {
		tmp_ptr = realloc(str_ptr, str_len + len + 1);
		if (!tmp_ptr) {
			return ERR;
		}
		str_ptr = (char *)tmp_ptr;
		str_len += len + 1;
	}

	memcpy(str_ptr + str_idx, buf + start, len);
	str_idx += len;

	return OK;
}

static int process_buf(const char *buf, size_t size)
{
	int i;
	static int st;
	char c;
	int last = 0;
	int err;

	assert(size != 0);

	/* FSM */
	for (i = 0; i < size; i++) {
		c = buf[i];
		switch (st) {
		case 0:
			if (c == 10 || c == 13) {
				assert(str_idx < str_len);
				st = 1;
				err = buf_to_str(buf, last, i);
				if (err != OK) {
					return err;
				}
				str_ptr[str_idx] = '\0';
				err = parse(str_ptr);
				if (err != OK) {
					/* only warning */
				}
				str_idx = 0;
			}
			break;
		case 1:
			if (c != 10 && c != 13) {
				last = i;
				st = 0;
			}
			break;
		default:
			/* this shouldn't happen */
			assert(0);
		}
	}

	if (st == 0) {
		err = buf_to_str(buf, last, size);
		if (err != OK) {
			return err;
		}
	}

	return OK;
}

static int process_fd(int fd)
{
	ssize_t size;
	char buf[MAX_READ_BUFSIZ];
	int err;
	int result = ERR;

	str_ptr = (char *)malloc(STR_BUFSIZ);
	if (!str_ptr) {
		return ERR;
	}
	str_len = STR_BUFSIZ;

	err = regcomp(&lsreg, lsreg_str, REG_EXTENDED);
	if (err < 0) {
		goto out;
	}
	err = regcomp(&lsregx, lsregx_str, REG_EXTENDED);
	if (err < 0) {
		goto out_reg;
	}

	while (1) {
		size = read(fd, buf, sizeof(buf));
		if (size < 0) {
			perror("read");
			break;
		}

		if (size == 0) {
			/* EOF */
			result = OK;
			break;
		}

		err = process_buf(buf, (size_t)size);
		if (err != OK) {
			break;
		}
	}

	regfree(&lsregx);
out_reg:
	regfree(&lsreg);
out:
	free(str_ptr);

	return result;
}

static int process_file(const char *file)
{
	int fd;
	int result = ERR;

	if (access(file, R_OK) == -1) {
		perror("access");
		return ERR;
	}

	fd = open(file, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		perror("open");
		return ERR;
	}

	result = process_fd(fd);
	close(fd);

	if (result != OK) {
		printf("ERROR: can't process file %s\n", file);
	}

	return result;

}

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

	node = node_from_path(path);
	if (!node) {
		return -EIO;
	}

	if (!node->data) {
		return -EINVAL;
	}

	len = strlen(node->data);
	if (offset > len) {
		return -EFAULT;
	}

	ptr = &node->data[offset];
	len = len - offset;
	if (len > size) {
		len = size;
	}

	memcpy(buf, ptr, len);

	return len;
}

static int fuse_listxattr(const char *path, char *buf, size_t size)
{
	(void)path;

	if (size < sizeof(SELINUX_XATTR)) {
		return -ERANGE;
	}

	strcpy(buf, SELINUX_XATTR);

	return sizeof(SELINUX_XATTR);
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

static void usage(const char *name)
{
#ifdef PACKAGE_STRING
	printf(PACKAGE_STRING "\n\n");
#endif /* PACKAGE_STRING */
	printf("Usage: %s [FILE] [FUSE_OPTIONS] MOUNT_POINT\n", name);
}

static struct fuse_operations fuse_oper = {
	.getattr = fuse_getattr,
	.readdir = fuse_readdir,
	.readlink = fuse_readlink,
	.open = fuse_open,
	.read = fuse_read,
	.listxattr = fuse_listxattr,
	.getxattr = fuse_getxattr,
};

int main(int argc, char **argv)
{
	int err;

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	/* XXX possible options ain't supported at the moment for stdin */
	if (argc == 2) {
		if ((strcmp(argv[1], "-h") == 0) ||
		    (strcmp(argv[1], "--help") == 0)) {
			usage(argv[0]);
			return 0;
		}

		/* stdin */
		err = process_fd(0);
		if (err != OK) {
			printf("ERROR: can't process stdin\n");
		}
	} else {
		err = process_file(argv[1]);
		argv++;
		argc--;
	}

	if (err != OK) {
		return 1;
	}

	return fuse_main(argc, argv, &fuse_oper);
}
