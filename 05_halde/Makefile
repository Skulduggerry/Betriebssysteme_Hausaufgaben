.PHONY: all clean

CC = gcc
CFLAGS = -std=c11 -pedantic -Wall -Werror -D_XOPEN_SOURCE=700 -g

all: test test-ref

clean:
	rm halde.o test test-ref

test: test.c halde.o
	$(CC) $(CFLAGS) halde.o test.c -o test

test-ref: halde-ref.o test.c
	$(CC) $(CFLAGS) halde-ref.o test.c -o test-ref

halde.o: halde.h halde.c
	$(CC) $(CFLAGS) -c halde.c