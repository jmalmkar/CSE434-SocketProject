# Single-folder Makefile (no src/include dirs)
CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O2
LDFLAGS := 

TARGETS := manager user disk

.PHONY: all clean
all: $(TARGETS)

manager: manager.c common.c protocol.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

user: user.c common.c protocol.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

disk: disk.c common.c protocol.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGETS) *.o
