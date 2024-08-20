#include <stdlib.h>
#include <unistd.h>

#include "util.h"

void *
xmalloc(size_t n)
{
	void *p;
	p = malloc(n);
	if (!p)
		abort();
	return p;
}

void *
xcalloc(size_t n, size_t sz)
{
	void *p;
	p = calloc(n, sz);
	if (!p)
		abort();
	return p;
}

void *
xrealloc(void *p, size_t n)
{
	void *np;
	np = realloc(p, n);
	if (!np)
		abort();
	return np;
}
