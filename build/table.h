#ifndef TABLE_H
#define TABLE_H

#include <assert.h>
#include <stddef.h>

#ifndef TABLE_INIT_SLOTS
#define TABLE_INIT_SLOTS 4
#endif
#ifndef TABLE_RESIZE_RATIO
#define TABLE_RESIZE_RATIO 70
#endif

static_assert(
	(TABLE_INIT_SLOTS & (TABLE_INIT_SLOTS - 1)) == 0,
	"TABLE_INIT_SLOTS must be a power of two"
);
static_assert(
	TABLE_RESIZE_RATIO > 0 && TABLE_RESIZE_RATIO < 100,
	"TABLE_RESIZE_RATIO must be between 0% and 100%"
);

/*
 * We need there to be at least one free slot at all times, else there will be
 * infinite loops. So, n * ratio% < n - 1 should be satisfied so that the table
 * will always be resized before it completely fills up.
 */
static_assert(
	TABLE_INIT_SLOTS * TABLE_RESIZE_RATIO < (TABLE_INIT_SLOTS - 1) * 100,
	"too few initial slots, or ratio is too large"
);

struct TableEntry {
	char *key;
	void *val;
};

struct Table {
	size_t n_slots; /* 2^k */
	size_t n_filled;
	size_t n_tomb; /* address of this field is tombstone */
	struct TableEntry *slots;
	struct TableEntry *more; /* unused half to avoid expensive copy */
};

void
table_init(struct Table *tbl);
void
table_destroy(struct Table *tbl);
void
table_resize(struct Table *tbl);
void *
table_insert(struct Table *tbl, const char *key, void *val);
int
table_delete(struct Table *tbl, const char *key);
void **
table_find(struct Table *tbl, const char *key);

#define TABLE_ITER(_tbl, _it)                    \
	for (struct TableEntry *_it = (_tbl)->slots; \
	     _it < (_tbl)->slots + (_tbl)->n_slots;  \
	     it++)
#define TABLE_ITER_SKIP_INVALID(_tbl, _it)              \
	if (!it->key || it->key == (char *)&(_tbl)->n_tomb) \
		continue;

#endif
