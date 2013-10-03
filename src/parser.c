/* parser.c
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
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hash.h"
#include "months.h"
#include "node.h"
#include "parser.h"
#include "tools.h"
#include "log.h"

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
#define R_MODE "([-rwxsStT]{9,9})"
#define R_XMODE "[t@+.]?"
#define R_USR "([0-9A-Za-z_-]+)"
#define R_GRP "([0-9A-Za-z_-]+)"
#define R_SIZ "([0-9]+|[0-9]+,[ \t]+[0-9]+)"
#define R_MONTH "([^ \t]+)"
#define R_DATE "([1-3]?[0-9][ \t]+[0-9]{4,4}|[1-3]?[0-9][ \t]+[0-2]?[0-9]:[0-5][0-9])"
#define R_SELINUX "([0-9a-zA-Z:_.-]+)"
#define R_NAME "(.+)"
#define R_BS R_SPACE_OPT R_NUM_OPT R_SPACE_OPT
#define R_SIZ_TOOLBOX "([0-9]+|[0-9]+,[ \t]+[0-9]+|[ \t])"
#define R_DATE_TOOLBOX "([0-9]{4,4}-[0-9]{2,2}-[0-9]{2,2})"
#define R_TIME_TOOLBOX "([0-2][0-9]:[0-5][0-9])"


static void node_set_type(lsnode_t *, const char *);
static void node_set_mode(lsnode_t *, const char *);
static void node_set_usr(lsnode_t *, const char *);
static void node_set_grp(lsnode_t *, const char *);
static void node_set_size(lsnode_t *, const char *);
static void node_set_month(lsnode_t *, const char *);
static void node_set_time(lsnode_t *, const char *);
static void node_set_time_toolbox(lsnode_t *, const char *);
static void node_set_date_toolbox(lsnode_t *, const char *);
static void node_set_selinux(lsnode_t *, const char *);
static void node_set_name(lsnode_t *, const char *);

static lsnode_t *cwd;

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
static const char lsreg_str[] =
	"^" R_BS R_TYPE R_MODE R_XMODE R_SPACE R_NUM R_SPACE R_USR R_SPACE
	R_GRP R_SPACE R_SIZ R_SPACE R_MONTH R_SPACE R_DATE R_SPACE R_NAME "$";
static const handler_t lsreg_cb[MATCH_NUM] = {NULL, node_set_type,
	node_set_mode, node_set_usr, node_set_grp, node_set_size,
	node_set_month, node_set_time, node_set_name,};

/* lsrega - regex for Android's toolbox */
/* 1 - file type
 * 2 - file mode (rwx)
 * 3 - owner
 * 4 - group
 * 5 - size or major, minor (empty for directories)
 * 6 - date
 * 7 - time
 * 8 - file name
 */
static regex_t lsrega;
static const char lsrega_str[] =
	"^" R_BS R_TYPE R_MODE R_XMODE R_SPACE R_USR R_SPACE
	R_GRP R_SPACE R_SIZ_TOOLBOX R_SPACE R_DATE_TOOLBOX R_SPACE
	R_TIME_TOOLBOX R_SPACE R_NAME "$";
static const handler_t lsrega_cb[MATCH_NUM] = {NULL, node_set_type,
	node_set_mode, node_set_usr, node_set_grp, node_set_size,
	node_set_date_toolbox, node_set_time_toolbox, node_set_name,};

/* lsregx - regex for ls -lZ and ls -lRZ */
/* 1 - file type
 * 2 - file mode (rwx)
 * 3 - owner
 * 4 - group
 * 5 - selinux context
 * 6 - file name
 */
static regex_t lsregx;
static const char lsregx_str[] =
	"^" R_TYPE R_MODE R_XMODE R_SPACE R_USR R_SPACE R_GRP R_SPACE R_SELINUX
	R_SPACE R_NAME "$";
static const handler_t lsregx_cb[MATCH_NUM] = {NULL, node_set_type,
	node_set_mode, node_set_usr, node_set_grp, node_set_selinux,
	node_set_name,};

static struct {
	/* compiled regexp */
	regex_t *reg;
	/* string representation of regexp */
	const char *str;
	/* table of callback functions */
	const handler_t *cb;
} lsreg_tbl[] = {
	{ &lsreg, lsreg_str, lsreg_cb },
	{ &lsrega, lsrega_str, lsrega_cb },
	{ &lsregx, lsregx_str, lsregx_cb },
};

/* FSM state */
static int fsm_st;
/* tmp buffer for processing files line by line */
static char *str_ptr;
static size_t str_len;
static size_t str_idx;

static int node_insert(lsnode_t *node)
{
	assert(cwd != NULL);

	/* TODO: check whether node exists in the cwd
	 *       if yes, add absent information*/

	node->next = cwd->entry;
	cwd->entry = node;

	return OK;
}

static void node_set_type(lsnode_t *node, const char * const type)
{
	static const struct {
		char key;
		mode_t value;
	} type_map[] = {
		{'-', S_IFREG},
		{'b', S_IFBLK},
		{'c', S_IFCHR},
		{'d', S_IFDIR},
		{'l', S_IFLNK},
		{'p', S_IFIFO},
		{'s', S_IFSOCK},
	};

	size_t i;
	mode_t s_if;
	char c;

	assert(type != NULL);
	assert(cwd != NULL);
	assert((node->mode & S_IFMT) == 0);

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

	if (s_if != 0) {
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
	} else if (mode[2] == 'S') {
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
	} else if (mode[5] == 'S') {
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
	} else if (mode[8] == 't') {
		st_mode |= S_IXOTH;
		st_mode |= S_ISVTX;
	} else if (mode[8] == 'T') {
		st_mode |= S_ISVTX;
	}

	node->mode &= S_IFMT;
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
	long long st_size;
	unsigned long st_rdev;

	assert(size != NULL);

	st_size = strtoll(size, &endptr, 10);
	assert(endptr != NULL);

	if (*endptr == '\0') {
		node->size = (off_t)st_size;
	} else if (*endptr == ',') {
		/* assume this is major, minor */
		do {
			++endptr;
		} while (*endptr == ' ' || *endptr == '\t');

		if (*endptr != '\0' && st_size < (1 << 8)) {
			st_rdev = (unsigned long)st_size;
			node->rdev = st_rdev << 8;
			st_rdev = strtoul(endptr, &endptr, 10);
			if (*endptr == '\0' && st_rdev < (1U << 8)) {
				node->rdev |= st_rdev;
			} else {
				node->rdev = 0;
			}
		}
	}
}

static void node_set_month(lsnode_t *node, const char * const month)
{
	size_t i;

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

static void node_set_time_toolbox(lsnode_t *node, const char * const time2)
{
	char *endptr = NULL;
	long hour, min;

	assert(time2 != NULL);
	assert(strlen(time2) == 5);

	hour = strtol(time2, &endptr, 10);
	if (endptr - time2 != 2 || *endptr != ':') {
		return;
	}

	min = strtol(time2 + 3, &endptr, 10);
	if (*endptr != '\0') {
		return;
	}

	node->time += (time_t)(hour * 3600 + min * 60);
}

static void node_set_date_toolbox(lsnode_t *node, const char * const date)
{
	struct tm t;
	time_t unix_time;
	char *endptr = NULL;

	assert(date != NULL);
	assert(strlen(date) == 10);

	memset(&t, 0, sizeof(t));

	t.tm_year = strtol(date, &endptr, 10);
	if (endptr - date != 4) {
		return;
	}
	t.tm_mon = strtol(date + 5, &endptr, 10);
	if (endptr - date != 7) {
		return;
	}
	--t.tm_mon;
	t.tm_mday = strtol(date + 8, &endptr, 10);
	if (*endptr != '\0') {
		return;
	}

	unix_time = mktime(&t);
	if (unix_time >= 0) {
		node->time += unix_time;
	}
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

static int parse_line(const char * const s, const regex_t * const reg,
		      const handler_t h_tbl[])
{
	regmatch_t match[MATCH_NUM];
	int i;
	size_t len = strlen(s);
	size_t sub_len;
	char tmp[len + 1];
	lsnode_t *node;

	if (regexec(reg, s, MATCH_NUM, match, 0) == REG_NOMATCH ) {
		return ERR;
	}

	LOGD("parsed: %s", s);

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

			LOGD("%d: %s", i, tmp);

			if (h_tbl[i] != NULL) {
				h_tbl[i](node, tmp);
			}
		}
	}

	if (node_insert(node) != OK) {
		/* this object already exists in the tree */
		node_free(node);
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
	size_t i;
	int err;

	for (i = 0; i < ARRAY_SIZE(lsreg_tbl); i++) {
		err = parse_line(line, lsreg_tbl[i].reg, lsreg_tbl[i].cb);
		if (err == OK) {
			break;
		}
	}

	if (err != OK) {
		if (is_dir(line)) {
			/* remove last ':' */
			i = strlen(line);
			line[i - 1] = '\0';
			/* TODO: check return value */
			chcwd(line);
		} else {
			LOGD("not parsed: %s", line);
		}
	}

	return OK;
}

static int buf_to_str(const char * const buf, size_t start, size_t end)
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

static int process_buf(const char * const buf, size_t size)
{
	size_t i;
	char c;
	size_t last = 0;
	int err;

	assert(size != 0);

	/* FSM */
	for (i = 0; i < size; i++) {
		c = buf[i];
		switch (fsm_st) {
		case 0:
			if (c == 10 || c == 13) {
				assert(str_idx < str_len);
				fsm_st = 1;
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
				fsm_st = 0;
			}
			break;
		default:
			/* this shouldn't happen */
			assert(0);
		}
	}

	if (fsm_st == 0) {
		err = buf_to_str(buf, last, size);
		if (err != OK) {
			return err;
		}
	}

	return OK;
}

static void clear_state(void)
{
	cwd = node_get_root();
	fsm_st = 0;
	str_idx = 0;
}

int parse_fd(int fd)
{
	ssize_t size;
	char buf[MAX_READ_BUFSIZ];
	int err;
	int result = ERR;

	clear_state();

	while (1) {
		size = read(fd, buf, sizeof(buf));
		if (size < 0) {
			LOGE("read: %s", strerror(errno));
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

	return result;
}

int parse_file(const char * const file)
{
	int err;
	int fd;

	fd = open(file, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		LOGE("open: %s", strerror(errno));
		return ERR;
	}

	err = parse_fd(fd);
	close(fd);

	return err;
}

int parser_init(void)
{
	int err = 0;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(lsreg_tbl); i++) {
		err = regcomp(lsreg_tbl[i].reg, lsreg_tbl[i].str, REG_EXTENDED);
		if (err < 0) {
			LOGE("Can't process regular expression #%u",
			     (unsigned int)i);
			break;
		}
	}

	if (err != 0) {
		while (i > 0) {
			--i;
			regfree(lsreg_tbl[i].reg);
		}
		return ERR;
	}

	str_ptr = (char *)malloc(STR_BUFSIZ);
	if (!str_ptr) {
		LOGE("Can't allocate memory");
		parser_destroy();
		return ERR;
	}
	str_len = STR_BUFSIZ;

	return OK;
}

void parser_destroy(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(lsreg_tbl); i++) {
		regfree(lsreg_tbl[i].reg);
	}

	if (str_ptr) {
		free(str_ptr);
	}
}
