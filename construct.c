/* Single file build system (plus header) */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include "construct.h"

// Malloc wrappers {{{

static void *
malloc_s(size_t n)
{
	void *p;
	p = malloc(n);
	if (!p)
		abort();
	return p;
}

static void *
calloc_s(size_t n, size_t sz)
{
	void *p;
	p = calloc(n, sz);
	if (!p)
		abort();
	return p;
}

static void *
realloc_s(void *p, size_t n)
{
	void *np;
	np = realloc(p, n);
	if (!np)
		abort();
	return np;
}

static char *
strdup_s(const char *s)
{
	char *ns;
	ns = strdup(s);
	if (!ns)
		abort();
	return ns;
}

// }}}

// Queues {{{

/* See https://github.com/tux314159/queuebench;
 * these are really, really fast. */

void
queue_init(struct Queue *q, size_t size)
{
	q->base = malloc_s(2 * (size + 1) * sizeof(*q->base));
	q->in = q->base;
	q->base2 = q->in + size;
	q->out = q->base2;
	q->inp = q->in;
	q->outp = q->out;
}

void
queue_destroy(struct Queue *q)
{
	free(q->base < q->base2 ? q->base : q->base2);
}

void
queue_push(struct Queue *q, void *val)
{
	*(q->inp++) = val;
}

void *
queue_pop(struct Queue *q)
{
	void **t;
	if (q->outp == q->out) {
		q->outp = q->base;
		q->out = q->inp;
		q->in = q->base2;
		q->inp = q->base2;
		t = q->base;
		q->base = q->base2;
		q->base2 = t;
	}
	return *(q->outp++);
}

size_t
queue_len(struct Queue *q)
{
	return (size_t)((q->inp - q->in) + (q->out - q->outp));
}

void
queue_clear(struct Queue *q)
{
	q->inp = q->in;
	q->out = q->outp;
}

// }}}

// Dynamic arrays {{{

void
array_init(struct Array *arr)
{
	arr->len = 0;
	arr->_cap = 1;
	arr->data = malloc_s(sizeof(*arr->data));
}

void
array_push(struct Array *arr, void *item)
{
	arr->_cap *= 2;
	arr->data = realloc_s(arr->data, arr->_cap * sizeof(*arr->data));
	arr->data[arr->len++] = item;
}

void *
array_pop(struct Array *arr)
{
	return arr->data[--arr->len];
}

void
array_destroy(struct Array *arr)
{
	free(arr->data);
}

// }}}

// Hashtables {{{

static uint32_t
djb2(const unsigned char *str)
{
	uint32_t hash = 5381;
	unsigned char c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c;

	return hash;
}

int
table_init(struct Table *tbl)
{
	tbl->n_slots = TABLE_INIT_SLOTS;
	tbl->n_filled = 0;
	tbl->n_tomb = 0;
	tbl->slots = calloc(2 * TABLE_INIT_SLOTS, sizeof(*tbl->slots));
	if (!tbl->slots)
		goto cleanup_fail;
	return 0;

cleanup_fail:
	return -1;
}

void
table_destroy(struct Table *tbl)
{
	for (size_t i = 0; i < tbl->n_slots; i++) {
		if (!tbl->slots[i].key || tbl->slots[i].key == (char *)&tbl->n_tomb)
			continue;
		free(tbl->slots[i].key);
	}
}

static int
_table_resize(struct Table *tbl)
{
	// Just double the capacity of the table

	uint32_t slot;
	const size_t old_cap = tbl->n_slots;
	struct TableEntry *new_slots;

	tbl->n_slots <<= 1;
	new_slots = calloc(2 * tbl->n_slots, sizeof(*new_slots));
	if (!new_slots)
		goto cleanup_fail;

	for (size_t i = 0; i < old_cap; i++) {
		if (!tbl->slots[i].key || tbl->slots[i].key == (char *)&tbl->n_tomb)

			continue;
		// Find new slot
		slot = djb2((unsigned char *)tbl->slots[i].key) &
			(tbl->n_slots - 1); /* hash % n */
		while (new_slots[slot].key)
			slot = (slot + 1) & (tbl->n_slots - 1);
		new_slots[slot].key = tbl->slots[i].key;
		new_slots[slot].val = tbl->slots[i].val;
	}
	free(tbl->slots);
	tbl->slots = new_slots;
	tbl->n_tomb = 0;

	return 0;
cleanup_fail:
	return -1;
}

static void
_table_exhume(struct Table *tbl)
{
	struct TableEntry *tmp;
	size_t slot;

	memset(tbl->more, 0, tbl->n_slots * sizeof(*tbl->more));

	for (size_t i = 0; i < tbl->n_slots; i++) {
		if (!tbl->slots[i].key || tbl->slots[i].key == (char *)&tbl->n_tomb)
			continue;
		/* Find new slot */
		slot = djb2((unsigned char *)tbl->slots[i].key) &
			(tbl->n_slots - 1); /* hash % n */
		while (tbl->more[slot].key)
			slot = (slot + 1) & (tbl->n_slots - 1);
		tbl->more[slot].key = tbl->slots[i].key;
		tbl->more[slot].val = tbl->slots[i].val;
	}
	tbl->n_tomb = 0;

	tmp = tbl->more;
	tbl->more = tbl->slots;
	tbl->slots = tmp;
}

static inline struct TableEntry *
_table_find(struct Table *tbl, const char *key)
{
	uint32_t slot;

	slot = djb2((const unsigned char *)key) & (tbl->n_slots - 1);
	while (tbl->slots[slot].key &&
	       (tbl->slots[slot].key == (char *)&tbl->n_tomb ||
	        strcmp(tbl->slots[slot].key, key))) {
		slot = (slot + 1) & (tbl->n_slots - 1);
	}
	if (tbl->slots[slot].key)
		return tbl->slots + slot;
	else
		return NULL;
}

int
table_insert(struct Table *tbl, const char *key, void *val)
{
	uint32_t slot;
	struct TableEntry *ent;

	/* If key already exists then update, else insert */
	ent = _table_find(tbl, key);
	if (ent) {
		ent->val = val;
	} else {
		if (100 * tbl->n_filled / tbl->n_slots > TABLE_RESIZE_RATIO)
			if (_table_resize(tbl))
				goto cleanup_fail;
		if (100 * (tbl->n_filled + tbl->n_tomb) / tbl->n_slots >
		    TABLE_RESIZE_RATIO)
			_table_exhume(tbl);

		slot = djb2((const unsigned char *)key) &
			(tbl->n_slots - 1); /* hash % n */
		while (tbl->slots[slot].key &&
		       tbl->slots[slot].key != (char *)&tbl->n_tomb)
			slot = (slot + 1) & (tbl->n_slots - 1);

		if (tbl->slots[slot].key == (char *)&tbl->n_tomb)
			tbl->n_tomb--;
		tbl->slots[slot].key = malloc_s(strlen(key) + 1);
		tbl->slots[slot].val = val;
		if (!tbl->slots[slot].key) {
			free(tbl->slots[slot].key);
			goto cleanup_fail;
		}
		strcpy(tbl->slots[slot].key, key);

		tbl->n_filled++;
	}

	return 0;

cleanup_fail:
	return -1;
}

int
table_delete(struct Table *tbl, const char *key)
{
	struct TableEntry *addr;

	addr = _table_find(tbl, key);
	if (!addr)
		return -1;
	free(addr->key);
	addr->key = (char *)&tbl->n_tomb;
	tbl->n_filled--;
	tbl->n_tomb++;

	return 0;
}

void **
table_find(struct Table *tbl, const char *key)
{
	struct TableEntry *addr;

	addr = _table_find(tbl, key);
	if (!addr)
		return NULL;
	else
		return &addr->val;
}

// }}}

// String wrangling {{{

char *
format_cmd(const char *s, const char *name, struct Array *deps)
{
	char *res_buf;
	size_t res_len;
	FILE *res;

	int special = 0, escaped = 0;

	res = open_memstream(&res_buf, &res_len);
	if (!res)
		abort();

	for (const char *c = s; c < s + strlen(s); c++) {
		if (special) {
			special = 0;
			switch (*c) {
			case '@':
				fprintf(res, "%s", name);
				break;
			case '<':
				fprintf(res, "%s", (char *)deps->data[0]);
				break;
			case '^':
				for (size_t i = 0; i < deps->len; i++)
					fprintf(res, "%s ", (char *)deps->data[i]);
				break;
			}
		} else {
			switch (*c) {
			case '\\':
				escaped = 1;
				continue;
			case '$':
				special = 1;
				break;
			default:
				fprintf(res, "%c", *c);
				break;
			}
		}
		escaped = 0;
	}

	fclose(res);
	return res_buf;
}

// }}}

// Build system graph {{{

void
graph_init(struct Depgraph *graph)
{
	graph->n_targets = 0;
	table_init(&graph->targets);
}

// WARNING: must end arglist with null!
struct Target *
make_target(const char *name, const char *cmd, ...)
{
	va_list args;
	char *arg;
	struct Target *target;

	target = malloc_s(sizeof(*target));
	target->name = strdup_s(name);
	array_init(&target->deps);
	array_init(&target->codeps);

	target->n_sat_dep = 0;
	target->visited = 0;

	va_start(args, cmd);
	while ((arg = va_arg(args, char *))) {
		array_push(&target->deps, arg);
	}

	if (cmd) {
		target->cmd = format_cmd(cmd, name, &target->deps);
	} else {
		target->cmd = NULL;
	}

	return target;
}

void
add_target(struct Depgraph *graph, struct Target *target)
{
	table_insert(&graph->targets, target->name, target);
	graph->n_targets++;
}

void
build_graph(struct Depgraph *graph, const char *final)
{
	struct Queue queue;
	struct Array leaves;
	struct Target **final_targ;
	struct stat sb;

	queue_init(&queue, graph->n_targets);
	array_init(&leaves);

	// BFS from the final target
	final_targ = (struct Target **)table_find(&graph->targets, final);
	if (!final_targ)
		die("Bad target: %s", final);
	queue_push(&queue, *final_targ);
	(*final_targ)->visited = 1;
	while (queue_len(&queue)) {
		struct Target *targ, *new;

		targ = queue_pop(&queue);
		if (targ->deps.len == 0)
			array_push(&leaves, targ); // collate leaves

		for (size_t i = 0; i < targ->deps.len; i++) {
			struct Target **c;
			c = (struct Target **)
				table_find(&graph->targets, targ->deps.data[i]);
			if (!c) {
				// If sought-after dependency doesn't exist, it may
				// be a source file, try to find and add it
				if (stat(targ->deps.data[i], &sb) == -1)
					die("Bad target: %s", (char *)targ->deps.data[i]);
				new = make_target(targ->deps.data[i], NULL, NULL);
				add_target(graph, new);
				c = &new;
			}
			array_push(&(*c)->codeps, targ); // fill up codeps

			if (!(*c)->visited) {
				queue_push(&queue, *c);
				(*c)->visited = 1;
			}
		}
	}

	// Multisource BFS from each source
	// NOTE: visited is flipped
	queue_clear(&queue);
	for (size_t i = 0; i < leaves.len; i++) {
		queue_push(&queue, leaves.data[i]); // start from each leaf
		((struct Target *)(leaves.data[i]))->visited = 0;
	}

	while (queue_len(&queue)) {
		struct Target *targ;
		struct timespec targ_mtim;
		int out_of_date;

		// For every target depending on us we keep track of the
		// number of satisfied targets and only build it if all
		// its targets are satisfied
		targ = queue_pop(&queue);
		if (targ->n_sat_dep < targ->deps.len) {
			queue_push(&queue, targ); // push it back to the back
			continue;
		}

		// Check if any dependencies are younger
		if (stat(targ->name, &sb) == -1) { // doesn't exist
			targ_mtim.tv_sec = 0;
			targ_mtim.tv_nsec = 0;
		} else {
			targ_mtim = sb.st_mtim;
		}

		out_of_date = 0;
		for (size_t i = 0; i < targ->deps.len; i++) {
			struct Target *dep;
			dep = *(struct Target **)
					  table_find(&graph->targets, targ->deps.data[i]);
			out_of_date |= stat(dep->name, &sb) == -1;
			out_of_date |= targ_mtim.tv_sec < sb.st_mtim.tv_sec;
			out_of_date |=
				(targ_mtim.tv_sec == sb.st_mtim.tv_sec &&
			     targ_mtim.tv_nsec < sb.st_mtim.tv_nsec);
		}

		if (out_of_date && targ->cmd) {
			log(msgt_raw, "%s", targ->cmd);
			system(targ->cmd);
		}

		for (size_t i = 0; i < targ->codeps.len; i++) {
			struct Target *c;

			c = targ->codeps.data[i];
			c->n_sat_dep++;

			if (!c->visited)
				continue;
			c->visited = 0;
			queue_push(&queue, c);
		}
	}
	queue_destroy(&queue);
	array_destroy(&leaves);
}

// }}}

#define needs(...) __VA_ARGS__,
#define nodeps
#define target(name, deps, cmd) \
	add_target(&graph, make_target(name, cmd, deps NULL))

// clang-format off
int
main(int argc, char **argv)
{
	struct Depgraph graph;

	graph_init(&graph);
	target("testproj/main",
			needs("testproj/main.o", "testproj/hello.o", "testproj/mymath.o"),
			"cc -o $@ $^");
	target("testproj/main.o", needs("testproj/main.c", "testproj/hello.h"),
			"cc -c -o $@ $<");
	target("testproj/hello.o", needs("testproj/hello.c", "testproj/hello.h"),
			"cc -c -o $@ $<");
	target("testproj/mymath.o", needs("testproj/mymath.c", "testproj/mymath.h"),
			"cc -c -o $@ $<");
	build_graph(&graph, "testproj/main");
}
