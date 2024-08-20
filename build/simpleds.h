#ifndef INCLUDE_SIMPLEDS_H
#define INCLUDE_SIMPLEDS_H

#include <stddef.h>

/* See https://github.com/tux314159/queuebench;
 * these are really, really fast. */

struct Queue {
	void **in, **out, **inp, **outp, **base, **base2;
};

void
queue_init(struct Queue *q, size_t size);
void
queue_destroy(struct Queue *q);
void
queue_push(struct Queue *q, void *val);
void *
queue_pop(struct Queue *q);
size_t
queue_len(struct Queue *q);
void
queue_clear(struct Queue *q);

struct Array {
	size_t len;
	size_t _cap;
	void **data;
};

void
array_init(struct Array *arr);
void
array_push(struct Array *arr, void *item);
void *
array_pop(struct Array *arr);
void
array_destroy(struct Array *arr);

#endif
