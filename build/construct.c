#include "dsl.h"

#define CC "cc"
#define CFLAGS "-Wall -Wextra -pthread"

int
main(int argc, char **argv)
{
	construction_site(ctx);

	target("testproj/main",
			needs("testproj/main.o", "testproj/hello.o", "testproj/mymath.o"),
			executes(l CC" "CFLAGS" -o" _ targ() _ alldeps() _));
	target("testproj/main.o", needs("testproj/main.c", "testproj/hello.h"),
			executes(l CC" "CFLAGS" -c -o" _ targ() _ firstdep() _));
	target("testproj/hello.o", needs("testproj/hello.c", "testproj/hello.h"),
			executes(l CC" "CFLAGS" -c -o" _ targ() _ firstdep() _));
	target("testproj/mymath.o", needs("testproj/mymath.c"),
			executes(l CC" "CFLAGS" -c -o" _ targ() _ firstdep() _));

	construct("testproj/main", 2);
	log(msgt_info, "build complete", 0);
}
