#include <stdio.h>

#include "hello.h"

void
hello_world(const char *s, int a, int b)
{
	printf("%s, %d+%d=%d and %d*%d=%d.\n", s, a, b, a + b, a, b, a * b);
}
