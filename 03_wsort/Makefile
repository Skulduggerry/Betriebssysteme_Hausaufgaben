.PHONY: all clean

CC=gcc
CFLAGS=-std=c11 -pedantic -Wall -Werror -D_XOPEN_SOURCE=700

all: wsort

clean:
	rm -f wsort

wsort: wsort.c
	$(CC) $(CFLAGS) -o $@ $^
