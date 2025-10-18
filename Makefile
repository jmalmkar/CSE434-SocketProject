CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O2
LDFLAGS := 

TARGETS := manager user disk

.PHONY: all clean
all: $(TARGETS)

manager: manager.c protocol.c storage.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

user: user.c protocol.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

disk: disk.c protocol.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGETS) *.o

