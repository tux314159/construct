#include "construct.h"

construction_site

target("testproj/main",
		needs("testproj/main.o", "testproj/hello.o", "testproj/mymath.o"),
		"cc -o $@ $^ && sleep 0.1");
target("testproj/main.o", needs("testproj/main.c", "testproj/hello.h"),
		"cc -c -o $@ $< && sleep 0.1");
target("testproj/hello.o", needs("testproj/hello.c", "testproj/hello.h"),
		"cc -c -o $@ $< && sleep 0.1");
target("testproj/mymath.o", needs("testproj/mymath.c", "testproj/mymath.h"),
		"cc -c -o $@ $< && sleep 0.1");
construct("testproj/main", 2);

construction_done
