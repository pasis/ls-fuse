/* hash.h
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

#ifndef LS_FUSE_HASH_H
#define LS_FUSE_HASH_H

#include <assert.h>
#include <limits.h>

#if defined(_POSIX_LOGIN_NAME_MAX) && _POSIX_LOGIN_NAME_MAX > 32
#define MAX_KEY_LEN _POSIX_LOGIN_NAME_MAX
#else
#define MAX_KEY_LEN 32
#endif

#define HASH_TBL_SIZE 128U
#if HASH_TBL_SIZE & ~(HASH_TBL_SIZE - 1U) != HASH_TBL_SIZE
#error HASH_TBL_SIZE must be power of 2
#endif
#define HASH_TBL_MASK (HASH_TBL_SIZE - 1)

typedef struct _hash_t {
	char key[MAX_KEY_LEN];
	long value;
	struct _hash_t *next;
} hash_t;

typedef hash_t hash_tbl_t[HASH_TBL_SIZE];

static int hash_func(const char *s)
{
	int h = 0;

	assert(s != NULL);
	while (*s) {
		h = (h + *s) & HASH_TBL_MASK;
		++s;
	}

	return h;
}

/* XXX: debug */
#include <stdio.h>

static inline void hash_add(hash_tbl_t tbl, const char *key, long value)
{
	int i;

	i = hash_func(key);
	if (*tbl[i].key == 0) {
		strncpy(tbl[i].key, key, MAX_KEY_LEN);
		tbl[i].key[MAX_KEY_LEN - 1] = '\0';
		tbl[i].value = value;

	} else {
		/* not implemented yet */
		printf("hash_add: not implemented\n");
	}
}

static inline long hash_get(const hash_tbl_t tbl, const char *key)
{
	int i;

	i = hash_func(key);
	if (*tbl[i].key == 0) {
		return -1;
	}

	if (strncmp(tbl[i].key, key, MAX_KEY_LEN) == 0) {
		return tbl[i].value;
	}

	return -1;
}

#endif /* LS_FUSE_HASH_H */
