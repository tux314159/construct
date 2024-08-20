OBJS = util.o simpleds.o table.o build.o construct.o
CC = cc
CFLAGS = -Ibuild -std=c17 -pthread -D_POSIX_C_SOURCE=200809L

.SUFFIXES: .c .o

all: construct

construct: $(OBJS)
	$(CC) $(CFLAGS) -o construct $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

