/* months.h
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

#ifndef LS_FUSE_MONTHS_H
#define LS_FUSE_MONTHS_H

static struct {
	char *key;
	int val;
} month_tbl[] = {
	/* english */
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

	/* russian */
	{"Янв", 0},
	{"Фев", 1},
	{"Мар", 2},
	{"Апр", 3},
	{"Май", 4},
	{"Июн", 5},
	{"Июл", 6},
	{"Авг", 7},
	{"Сен", 8},
	{"Окт", 9},
	{"Ноя", 10},
	{"Дек", 11},
};

#endif /* LS_FUSE_MONTHS_H */
