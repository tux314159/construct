#include <stddef.h>
#include <string.h>

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

#define FROM_BUF(s) (s - sizeof(size_t))
#define TO_BUF(s) (s + sizeof(size_t))
#define BUF_LEN(s) ((size_t *)FROM_BUF(s))

#pragma clang diagnostic ignored "-Wcast-align" /* clangd stfu */

static Str
str_alloc_len(size_t len)
{
	char *s;
	s = xcalloc(sizeof(size_t) + len + 1, 1);
	return TO_BUF(s);
}

Str
str_alloc(void)
{
	return str_alloc_len(0);
}

void
str_free(Str s)
{
	free(FROM_BUF(s));
}

size_t
str_len(Str s)
{
	return *BUF_LEN(s);
}

size_t
str_relen(Str s)
{
	size_t len;
	len = strlen(s);
	*BUF_LEN(s) = len;
	return len;
}

Str
str_growto(Str s, size_t len)
{
	char *ns = xrealloc(FROM_BUF(s), sizeof(size_t) + len + 1);
	return TO_BUF(ns);
}

Str
str_concat(Str restrict dest, Str restrict src)
{
	Str ns;
	size_t dest_len; /* might reallocate */
	dest_len = *BUF_LEN(dest);
	ns = str_growto(dest, dest_len + *BUF_LEN(src));
	memcpy(ns + dest_len, src, *BUF_LEN(src) + 1);
	*BUF_LEN(ns) = dest_len + *BUF_LEN(src);
	return ns;
}

Str
str_concatraw(Str restrict dest, const char *restrict src)
{
	Str ns;
	size_t src_len, dest_len;
	src_len = strlen(src);
	dest_len = *BUF_LEN(dest);
	ns = str_growto(dest, dest_len + src_len);
	memcpy(ns + dest_len, src, src_len + 1);
	*BUF_LEN(ns) = dest_len + src_len;
	return ns;
}

Str
str_merge(Str restrict dest, Str restrict src)
{
	Str ns;
	ns = str_concat(dest, src);
	str_free(src);
	return ns;
}

Str
str_fromraw(Str restrict s, const char *restrict buf)
{
	Str ns;
	ns = s ? s : str_alloc();
	*BUF_LEN(ns) = strlen(buf);
	ns = str_growto(ns, *BUF_LEN(ns));
	memcpy(ns, buf, *BUF_LEN(ns) + 1);
	return ns;
}

Str
str_dupl(Str s)
{
	Str ns;
	ns = str_alloc_len(*BUF_LEN(s));
	memcpy(ns, s, *BUF_LEN(s) + 1);
	*BUF_LEN(ns) = *BUF_LEN(s);
	return ns;
}
