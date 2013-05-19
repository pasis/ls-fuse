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
