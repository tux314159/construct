#include "dsl.h"

#define CC "cc"
#define CFLAGS "-Wall -Wextra -pthread"
#define compile_c_obj executes(l CC" "CFLAGS" -c -o" _ targ() _ firstdep() _)

int
main(int argc, char **argv)
{
	construction_site(ctx);

	target("testproj/main",
		needs("testproj/main.o", "testproj/hello.o", "testproj/mymath.o"),
		executes(l CC" "CFLAGS" -o" _ targ() _ alldeps() _));

	target("testproj/main.o", needs("testproj/main.c", "testproj/hello.h"),
		compile_c_obj);
	target("testproj/hello.o", needs("testproj/hello.c", "testproj/hello.h"),
		compile_c_obj);
	target("testproj/mymath.o", needs("testproj/mymath.c"),
		compile_c_obj);

	construct("testproj/main", 2);
	log(msgt_info, "build complete", 0);
	construction_done();
}
