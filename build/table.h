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

#endif
