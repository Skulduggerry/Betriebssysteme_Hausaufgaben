.PHONY: all clean

CC = gcc
CFLAGS = -std=c11 -pedantic -Wall -Werror -D_XOPEN_SOURCE=700

all: clash

clean:
	rm -f clash plist.o

clash: clash.c plist.o plist.h
	$(CC) $(CFLAGS) -o clash clash.c plist.o

plist.o: plist.c plist.h
	$(CC) $(CLAGS) -c plist.c
