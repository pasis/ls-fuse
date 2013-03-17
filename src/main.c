#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <assert.h>
#include <errno.h>
#include <fuse.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERR 1
#define OK 0

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define MATCH_NUM 10

struct lsnode {
	mode_t mode;
	uid_t uid;
	gid_t gid;
	off_t size;
	dev_t rdev;
	char *name;
	struct lsnode *next;
	struct lsnode *entry;
};

struct hash8 {
	char key;
	int val;
};

typedef struct lsnode lsnode_t;
typedef struct hash8 hash8_t;
typedef void (*handler_t)(lsnode_t *, const char *);

static void node_set_type(lsnode_t *, const char *);
static void node_set_mode(lsnode_t *, const char *);
static void node_set_name(lsnode_t *, const char *);

static lsnode_t root = {
	.mode = S_IFDIR | 0755,
	.name = "/",
};
static lsnode_t *cwd = &root;

static regex_t lsreg;
static char lsreg_str[] =
	"^([-bcdlpsS])([-rwxs]{9,9})[ \t]+[0-9]+[ \t]+([0-9A-Za-z]+)[ \t]+"
	"([0-9A-Za-z]+)[ \t]+([0-9]+|[0-9]+,[ \t]+[0-9]+)[ \t]+([^ \t]+)[ \t]+"
	"([1-3]?[0-9][ \t]+[0-2]?[0-9]:[0-5][0-9]|[1-3]?[0-9][ \t]+[0-9]{4,4})"
	"[ \t]+(.+)$";
static handler_t lsreg_tbl[MATCH_NUM] = {NULL, &node_set_type, &node_set_mode,
	NULL, NULL, NULL, NULL, NULL, &node_set_name,};


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

static void node_set_name(lsnode_t *node, const char * const name)
{
	assert(name != NULL);

	node->name = strdup(name);
}

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
		if (*tok == '\0' || strcmp(tok, ".") == 0) {
			/* do nothing, parent remains the same */
			/* TODO: handle '..' in path (doubly linked list?) */
		} else {
			node = parent->entry;
			found = 0;
			while (node) {
				if (node->name && !strcmp(tok, node->name)) {
					found = 1;
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

static void parse_line(char *s, const regex_t *reg, const handler_t h_tbl[])
{
	regmatch_t match[MATCH_NUM];
	int i;
	size_t len = strlen(s);
	size_t sub_len;
	char tmp[len + 1];
	lsnode_t *node;

	printf("parsing: %s\n", s);

	if (regexec(reg, s, MATCH_NUM, match, 0) == REG_NOMATCH ) {
		/* TODO: return ERR */
		return;
	}

	node = node_alloc();
	if (!node) {
		/* TODO: return ERR */
		return;
	}

	for(i = 1; i < MATCH_NUM; i++) {
		if (match[i].rm_so >= 0 && match[i].rm_eo >= match[i].rm_so) {
			sub_len = match[i].rm_eo - match[i].rm_so;
			if (sub_len > len) {
				continue;
			}
			strncpy(tmp, &s[match[i].rm_so], sub_len);
			tmp[sub_len] = '\0';
			printf("%d: %s\n", i, tmp);

			if (h_tbl[i] != NULL) {
				h_tbl[i](node, tmp);
			}
		}
	}
}

static int is_dir(const char * const s)
{
	size_t len;

	assert(s != NULL);

	len = strlen(s);
	if (len < 2) {
		return 0;
	}

	if ((s[0] == '/' || s[0] == '.') && s[len - 1] == ':') {
		return 1;
	}

	return 0;
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

static int parse(const char *buf, size_t size)
{
	size_t i = 0;
	size_t last = 0;
	size_t len;
	char s[1024];

	/* 1 - file type
	 * 2 - file mode (rwx)
	 * 3 - owner
	 * 4 - group
	 * 5 - size or major, minor
	 * 6 - month
	 * 7 - day and time or day and year
	 * 8 - file name
	 */
	regcomp(&lsreg, lsreg_str, REG_EXTENDED);

	while (i < size) {
		/* TODO: move get next str logic to a separated function */
		if (buf[i] == '\n' || buf[i] == '\r' || i == size - 1) {
			len = i - last;
			if (len > 0 && len < sizeof(s)) {
				strncpy(s, &buf[last], len);
				s[len] = '\0';
				if (is_dir(s)) {
					/* remove last ':' */
					s[len - 1] = '\0';
					chcwd(s);
				} else {
					parse_line(s, &lsreg, lsreg_tbl);
				}
			}
			last = i + 1;
		}
		i++;
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

static struct fuse_operations fuse_oper = {
	.getattr = fuse_getattr,
	.readdir = fuse_readdir,
};

int main(int argc, char **argv)
{
	if (argc < 2) {
		return 1;
	}
	if (process_file(argv[1]) != OK) {
		return 1;
	}
	return fuse_main(argc - 1, argv + 1, &fuse_oper);
}
