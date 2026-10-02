CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -O2 -g -D_POSIX_C_SOURCE=200809L

all: parent child

parent: src/parent.c src/common.h
	$(CC) $(CFLAGS) -o $@ src/parent.c

child: src/child.c src/common.h
	$(CC) $(CFLAGS) -o $@ src/child.c

clean:
	rm -f parent child

.PHONY: all clean
