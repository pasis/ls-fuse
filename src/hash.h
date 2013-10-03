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
#include <stdlib.h>

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

static inline void hash_add(hash_tbl_t tbl, const char *key, long value)
{
	int i;
	hash_t *h;

	i = hash_func(key);
	if (*tbl[i].key == 0) {
		strncpy(tbl[i].key, key, MAX_KEY_LEN);
		tbl[i].key[MAX_KEY_LEN - 1] = '\0';
		tbl[i].value = value;

	} else {
		h = &tbl[i];
		while (h->next) {
			h = h->next;
		}
		h->next = (hash_t *)malloc(sizeof(hash_t));
		h = h->next;
		memset(h, 0, sizeof(hash_t));
		/* h->key will be null-terminated as h->key[MAX_KEY_LEN] is
		 * set to 0 by memset */
		strncpy(h->key, key, MAX_KEY_LEN - 1);
		h->value = value;
	}
}

static inline long hash_get(const hash_tbl_t tbl, const char *key)
{
	int i;
	const hash_t *h;

	i = hash_func(key);
	if (*tbl[i].key == 0) {
		return -1;
	}

	h = &tbl[i];
	while (h) {
		if (strncmp(h->key, key, MAX_KEY_LEN) == 0) {
			return h->value;
		}
		h = h->next;
	}

	return -1;
}

static inline void hash_destroy(hash_tbl_t tbl)
{
	unsigned int i;
	hash_t *h;
	hash_t *next;

	for (i = 0; i < HASH_TBL_SIZE; i++) {
		h = tbl[i].next;
		while (h != NULL) {
			next = h->next;
			free(h);
			h = next;
		}
	}
}

#endif /* LS_FUSE_HASH_H */
