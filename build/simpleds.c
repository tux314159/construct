#include <stddef.h>

#include "simpleds.h"
#include "util.h"

void
queue_init(struct Queue *q, size_t size)
{
	q->base = xmalloc(2 * (size + 1) * sizeof(*q->base));
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
void
array_init(struct Array *arr)
{
	arr->len = 0;
	arr->_cap = 1;
	arr->data = xmalloc(sizeof(*arr->data));
}
void
array_push(struct Array *arr, void *item)
{
	if (arr->len == arr->_cap)
		arr->_cap *= 2;
	arr->data = xrealloc(arr->data, arr->_cap * sizeof(*arr->data));
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
