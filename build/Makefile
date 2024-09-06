OBJS = util.o simpleds.o table.o target.o build.o threadpool.o construct.o
CC = cc
CFLAGS = -g -Ibuild -std=c17 -pthread -D_POSIX_C_SOURCE=200809L -Wno-format-extra-args

.PHONY: all test clean

.SUFFIXES: .c .o

all: construct

test: construct
	(cd ..; rm -f testproj/*.o testproj/main; build/construct)

construct: $(OBJS)
	$(CC) $(CFLAGS) -o construct $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f construct $(OBJS)
