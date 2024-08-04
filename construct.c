#include "construct.h"

int
main(int argc, char **argv)
{
	construction_site(ctx);

	target("testproj/main",
			needs("testproj/main.o", "testproj/hello.o", "testproj/mymath.o"),
			"cc -o $@ x $^ && sleep 0.1");
	target("testproj/main.o", needs("testproj/main.c", "testproj/hello.h"),
			"cc -c -o $@ $< && sleep 0.1");
	target("testproj/hello.o", needs("testproj/hello.c", "testproj/hello.h"),
			"cc -c -o $@ $< && sleep 0.1");
	target("testproj/mymath.o", needs("testproj/mymath.c"),
			"cc -c -o $@ $< && sleep 0.1");
	//add_dep("testproj/mymath.o", "<nonexistent target> (remove me to build!)");
	log(msgt_info, "Done adding dependencies :)", 0);

	construct("testproj/main", 3);
}
