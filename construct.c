#include "construct.h"

int
main(int argc, char **argv)
{
	construction_site(ctx);

	target("testproj/main",
			needs("testproj/main.o", "testproj/hello.o", "testproj/mymath.o"),
			"cc -o $@ $^ && sleep 0.1");
	target("testproj/main.o", needs("testproj/main.c", "testproj/hello.h"),
			"cc -c -o $@ $< && sleep 0.1");
	target("testproj/hello.o", needs("testproj/hello.c", "testproj/hello.h"),
			"cc -c -o $@ $< && sleep 0.1");
	target("testproj/mymath.o", needs("testproj/mymath.c"),
			"cc -c -o $@ $< && sleep 0.1");
	add_dep("testproj/mymath.o", target("mymathx.h", nodeps, ""));

	construct("testproj/main", 2);
}
