#ifndef INCLUDE_CONSTRUCT_H
#define INCLUDE_CONSTRUCT_H

#include <stdatomic.h>
#include <stddef.h>

// Pretty-printing/logging {{{

/* ANSI escapes */
#define T_DIM "\x1b[2m"
#define T_BOLD "\x1b[1m"
#define T_ITAL "\x1b[3m"
#define T_LINE "\x1b[4m"
#define T_RED "\x1b[38;5;1m"
#define T_GREEN "\x1b[38;5;2m"
#define T_YELLOW "\x1b[38;5;3m"
#define T_BLUE "\x1b[38;5;4m"
#define T_NORM "\x1b[0m"

enum MsgT {
	msgt_info = 0,
	msgt_warn,
	msgt_err,
	msgt_raw,
	msgt_end,
};
static const char *_msgtstr[msgt_end] =
	{T_BLUE, T_YELLOW, T_RED}; // not a macro so we don't forget to update it

#define log(type, msg, ...)                              \
	do {                                                 \
		if (type == msgt_raw) {                          \
			printf(msg "\n", __VA_ARGS__);               \
			break;                                       \
		}                                                \
		printf(                                          \
			T_DIM "%s:%d: " T_NORM "%s" msg T_NORM "\n", \
			__FILE__,                                    \
			__LINE__,                                    \
			_msgtstr[type],                              \
			__VA_ARGS__                                  \
		);                                               \
		fflush(stdout);                                  \
	} while (0)

#define die(msg, ...)                    \
	do {                                 \
		log(msgt_err, msg, __VA_ARGS__); \
		exit(1);                         \
	} while (0);

// }}}

// Queues {{{

struct Queue {
	void **in, **out, **inp, **outp, **base, **base2;
};

/* Initialise a queue with max capacity. */
void
queue_init(struct Queue *q, size_t size);

/* Free a queue. */
void
queue_destroy(struct Queue *q);

/* Push item into the back of the queue. */
void
queue_push(struct Queue *q, void *val);

/* Pop item from the front of the queue. */
void *
queue_pop(struct Queue *q);

/* Get length of a queue. */
size_t
queue_len(struct Queue *q);

/* Clear a queue. */
void
queue_clear(struct Queue *q);

// }}}

// Dynamic arrays {{{
struct Array {
	size_t len;
	size_t _cap;
	void **data;
};

/* Initialise an array. */
void
array_init(struct Array *arr);

/* Push element to back of array. */
void
array_push(struct Array *arr, void *item);

/* Pop element from back of array. */
void *
array_pop(struct Array *arr);

/* Destroy an array. */
void
array_destroy(struct Array *arr);

// }}}

// Hashtables {{{

/*
 * Quick and dirty hash-tables, using linear probing. We'll just use djb2...
 * There are some tunables in the macros below.
 */

#define TABLE_INIT_SLOTS 4
#define TABLE_RESIZE_RATIO 70

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

/* Initialise a table. */
int
table_init(struct Table *tbl);

/* Deallocate a table. */
void
table_destroy(struct Table *tbl);

/* Find a value in a table, return a pointer to a pointer to it, and a NULL
 * pointer if it does not exist.
 * WARNING: If not NULL dereference this pointer immediately as it may be
 * shifted upon subsequent inserts! */
void **
table_find(struct Table *tbl, const char *key);

/* Insert item into table. Key must be null-terminated and can be ephermal,
 * val is simply a pointer and will not be copied. */
int
table_insert(struct Table *tbl, const char *key, void *val);

/* Remove item from table. */
int
table_delete(struct Table *tbl, const char *key);

// }}}

// Build system graph {{{

struct Target {
	char *name;
	char *cmd;
	struct Array deps;
	struct Array codeps; // NOTE: array of struct Target *
	atomic_size_t *n_sat_dep;
	char visited;
};

struct Depgraph {
	size_t n_targets;
	struct Table targets;
};

/* Initialise a dependency graph. */
void
graph_init(struct Depgraph *graph);

/* Arglist contains list of dependencies.
 * WARNING: must end arglist with null! */
struct Target *
make_target(const char *name, const char *cmd, ...);

/* Add target to a dependency graph */
void
add_target(struct Depgraph *graph, struct Target *target);

/* Build the dependency graph, checking modification timestamps */
void
build_graph(struct Depgraph *graph, const char *final, int max_jobs);

// }}}

#endif
