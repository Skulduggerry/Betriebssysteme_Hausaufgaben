#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "halde.h"

int main(int argc, char *argv[]) {
	printList();

	// malloc everything: list should be empty
	char *all = malloc(1024*1024 - 16);
	assert(all != NULL);
	memset(all, 1, 1024*1024 - 16); // initialize the entire memory with ones so we can check calloc later
	printList();
	free(all);
	printList();

	// malloc everything except for 33 bytes
	// this should leave an mblock with size of 1 in the unused list
	all = malloc(1024*1024 - 33);
	printList();
	free(all);
	printList();

	char *m1 = malloc(200*1024);
	printList();

	char *m2 = malloc(200*1024);
	printList();

	char *m3 = malloc(200*1024);
	printList();

	char *m4 = malloc(200*1024);
	printList();

	free(m1);
	printList();

	free(m2);
	printList();

	free(m3);
	printList();

	free(m4);
	printList();

	m1 = malloc(1);
	printList();

	m2 = malloc(2);
	printList();

	m3 = malloc(3);
	printList();

	m4 = malloc(4);
	printList();

	free(m1);
	printList();

	free(m2);
	printList();

	free(m3);
	printList();

	free(m4);
	printList();

	// Randfall: free auf einem NULL pointer funktioniert ohne Probleme
	free(NULL);

	// Randfall: malloc with size 0 should return NULL
	// assert will crash the program should the returned pointer not be NULL
	assert(NULL == malloc(0));

	// now test calloc
	// we set all data to 1 earlier so we can now check that it is all 0
	char *mem = calloc(1, 10);
	printList();
	for (int i = 0; i < 10; i++) {
		assert(mem[i] == 0);
	}

	// now we realloc into a bigger list
	// the first 10 elements should still be 0
	mem = realloc(mem, 20);
	printList();
	for (int i = 0; i < 10; i++) {
		assert(mem[i] == 0);
	}

	// now we realloc back into a smaller array
	// we should still have 0 set
	mem = realloc(mem, 1);
	printList();
	assert(*mem == 0);

	exit(EXIT_SUCCESS);
}
