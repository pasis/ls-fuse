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
#include <sys/mman.h>
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

#define ERR 1
#define OK 0

#ifndef FALSE
#define FALSE 0
#endif /* FALSE */
#ifndef TRUE
#define TRUE 1
#endif /* TRUE */

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* maximum number of regex matches */
#define MATCH_NUM 10

#define SELINUX_XATTR "security.selinux"

struct lsnode {
	mode_t mode;
	uid_t uid;
	gid_t gid;
	off_t size;
	dev_t rdev;
	int month;
	time_t time;
	char *name;
	char *data;
	struct lsnode *next;
	struct lsnode *entry;
};

struct hash8 {
	char key;
	int val;
};

struct hash_str {
	char *key;
	int val;
};

typedef struct lsnode lsnode_t;
typedef struct hash8 hash8_t;
typedef struct hash_str hash_str_t;
typedef void (*handler_t)(lsnode_t *, const char *);

static void node_set_type(lsnode_t *, const char *);
static void node_set_mode(lsnode_t *, const char *);
static void node_set_usr(lsnode_t *, const char *);
static void node_set_grp(lsnode_t *, const char *);
static void node_set_size(lsnode_t *, const char *);
static void node_set_month(lsnode_t *, const char *);
static void node_set_time(lsnode_t *, const char *);
static void node_set_name(lsnode_t *, const char *);

static lsnode_t root = {
	.mode = S_IFDIR | 0755,
	.name = "/",
};
static lsnode_t *cwd = &root;

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
	"^[ \t]*[0-9]*[ \t]*([-bcdlps])([-rwxsS]{9,9})[t@+.]?[ \t]+[0-9]+[ \t]+"
	"([0-9A-Za-z]+)[ \t]+([0-9A-Za-z]+)[ \t]+([0-9]+|[0-9]+,[ \t]+[0-9]+)"
	"[ \t]+([^ \t]+)[ \t]+([1-3]?[0-9][ \t]+[0-9]{4,4}|[1-3]?[0-9][ \t]+"
	"[0-2]?[0-9]:[0-5][0-9])[ \t]+(.+)$";
static handler_t lsreg_tbl[MATCH_NUM] = {NULL, &node_set_type, &node_set_mode,
	&node_set_usr, &node_set_grp, &node_set_size, &node_set_month,
	&node_set_time, &node_set_name,};


/*
static int process_file(const char *file)
{
	FILE *f;
	char buf[4096];

	f = fopen(file, "r");
	if (!f) {
		perror("fopen");
		return ERR;
	}

	while (1) {
		if (fgets(buf, sizeof(buf), f) == NULL) {
			break;
		}
		printf("%s\n", buf);
		parse(buf);
	}

	if (!feof(f)) {
		printf("error while reading file\n");
	}

	return OK;
}
*/

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

static void node_set_type(lsnode_t *node, const char * const type)
{
	static hash8_t type_map[] = {
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
			s_if = type_map[i].val;
			break;
		}
	}

	if (s_if) {
		node->mode |= s_if;
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

	pwd = getpwnam(owner);
	if (pwd) {
		node->uid = pwd->pw_uid;
	} else {
		/* if owner is numeric */
		uid = strtol(owner, &endptr, 10);
		assert(endptr != NULL);
		if (*endptr == '\0') {
			node->uid = uid;
		}
	}
}

static void node_set_grp(lsnode_t *node, const char * const group)
{
	struct group *grp;
	char *endptr;
	long gid;

	assert(group != NULL);

	grp = getgrnam(group);
	if (grp) {
		node->gid = grp->gr_gid;
	} else {
		/* if group is numeric */
		gid = strtol(group, &endptr, 10);
		assert(endptr != NULL);
		if (*endptr == '\0') {
			node->gid = gid;
		}
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
	static hash_str_t month_tbl[] = {
		{"Jan", 0},
		{"Feb", 1},
		{"Mar", 2},
		{"Apr", 3},
		{"May", 4},
		{"Jun", 5},
		{"Jul", 6},
		{"Aug", 7},
		{"Sep", 8},
		{"Oct", 9},
		{"Nov", 10},
		{"Dec", 11},
	};

	int i;

	assert(month != NULL);

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

/* node_from_path must be thread safe */
static lsnode_t *node_from_path(const char * const path)
{
	lsnode_t *parent;
	lsnode_t *node;
	char *tmp = strdup(path);
	char *tok;
	char *saveptr;
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

static int parse_line(char *s, const regex_t *reg, const handler_t h_tbl[])
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

static char *get_next_line(const char *buf, size_t size, char *s, size_t len)
{
	static size_t last;
	size_t i;
	size_t tmp_len;
	const char *tmp_ptr;

	i = last;

	while (i < size) {
		if (buf[i] == '\n' || buf[i] == '\r' || i == size - 1) {
			tmp_len = i - last;
			tmp_ptr = &buf[last];
			last = i + 1;
			if (tmp_len > 0 && tmp_len < len) {
				strncpy(s, tmp_ptr, tmp_len);
				s[tmp_len] = '\0';
				return s;
			}
		}
		i++;
	}

	return NULL;
}

static int parse(const char *buf, size_t size)
{
	size_t len;
	int err;
	char s[1024];

	regcomp(&lsreg, lsreg_str, REG_EXTENDED);

	while (get_next_line(buf, size, s, sizeof(s))) {
		err = parse_line(s, &lsreg, lsreg_tbl);

		if (err == ERR && is_dir(s)) {
			/* remove last ':' */
			len = strlen(s);
			s[len - 1] = '\0';
			chcwd(s);
		}
	}

	regfree(&lsreg);

	return OK;
}

static int process_file(const char *file)
{
	struct stat sb;
	int fd;
	int result = ERR;
	char *map;

	if (access(file, R_OK) == -1) {
		perror("access");
		return ERR;
	}
	fd = open(file, O_RDONLY);
	if (fd == -1) {
		perror("open");
		return ERR;
	}

	if (fstat(fd, &sb) == -1) {
		perror("fstat");
		goto out;
	}

	map = (char *)mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (mmap == MAP_FAILED) {
		perror("mmap");
		goto out;
	}

	result = parse(map, sb.st_size);

	if (munmap(map, sb.st_size) == -1) {
		perror("munmap");
	}

out:
	if (close(fd) == -1) {
		perror("close");
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

	stbuf->st_mode = node->mode;
	stbuf->st_nlink = 1;
	stbuf->st_size = node->size;
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

	parent = node_from_path(path);
	if (!parent) {
		return -ENOENT;
	}
	if (!(parent->mode & S_IFDIR)) {
		return -ENOTDIR;
	}

	if (filler(buf, ".", NULL, 0) == 1 ||
	    filler(buf, "..", NULL, 0) == 1) {
		return -EINVAL;
	}

	node = parent->entry;
	while (node) {
		if (node->name) {
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
	/* TODO: remove after implementation */
	(void)path;
	(void)buf;

	if (strcmp(name, SELINUX_XATTR) != 0) {
		/* ENOATTR is a synonym for ENODATA */
		return -ENODATA;
	}

	/* TODO: not implemented, just return error */
	return -ENODATA;
}

static void usage(const char *name)
{
#ifdef PACKAGE_STRING
	printf(PACKAGE_STRING "\n\n");
#endif /* PACKAGE_STRING */
	printf("Usage: %s <FILE> [FUSE_OPTIONS] <MOUNT_POINT>\n", name);
}

static struct fuse_operations fuse_oper = {
	.getattr = fuse_getattr,
	.readdir = fuse_readdir,
	.readlink = fuse_readlink,
	.listxattr = fuse_listxattr,
	.getxattr = fuse_getxattr,
};

int main(int argc, char **argv)
{
	if (argc < 3) {
		usage(argv[0]);
		return 1;
	}
	if (process_file(argv[1]) != OK) {
		fprintf(stderr, "Unrecognized file format!\n");
		return 1;
	}
	return fuse_main(argc - 1, argv + 1, &fuse_oper);
}
