#include "halde.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/// Magic value for occupied memory chunks.
#define MAGIC ((void*)0xbaadf00d)

/// Size of the heap (in bytes).
#define SIZE (1024*1024*1)

/// Memory-chunk structure.
struct mblock {
	struct mblock *next;
	size_t size;
	char memory[];
};

/// Heap-memory area.
static char memory[SIZE];

/// Pointer to the first element of the free-memory list.
static int initialized = 0;
static struct mblock *head;

/// Helper function to visualise the current state of the free-memory list.
void printList(void) {
	struct mblock *lauf = head;

	// Empty list
	if (head == NULL) {
		char empty[] = "(empty)\n";
		write(STDERR_FILENO, empty, sizeof(empty) - 1);
		return;
	}

	// Print each element in the list
	const char fmt_init[] = "(off: %7zu, size:: %7zu)";
	const char fmt_next[] = " --> (off: %7zu, size:: %7zu)";
	const char *fmt = fmt_init;
	char buffer[sizeof(fmt_next) + 2 * 7];

	while (lauf) {
		size_t n = snprintf(buffer, sizeof(buffer), fmt
		                    , (uintptr_t) lauf - (uintptr_t) memory, lauf->size);
		if (n) {
			write(STDERR_FILENO, buffer, n);
		}

		lauf = lauf->next;
		fmt = fmt_next;
	}
	write(STDERR_FILENO, "\n", 1);
}

void *malloc(size_t size) {
	if (0 == size) {
		return NULL;
	}

	// ATTENTION: normally we would make sure to align our memory
	// HOWEVER: the halde-ref.o doesn't do memory alignment either so I took it out
	// make sure we are always aligned to 8 bytes
	// this is important for modern CPUs
	//size = (size + 7) & ~7;

	if (!initialized) {
		// initialize the head pointer
		initialized = 1;

		head = (struct mblock *) memory; // the head points to the beginning of the memory
		head->next = NULL; // there is no next memory segment
		head->size = sizeof(memory) - sizeof(struct mblock);
		// from the total memory we need some for storing mblock information
	}

	// search for a memory segment that is large enough
	struct mblock *prev = NULL;
	struct mblock *current = head;
	while (NULL != current && current->size < size) {
		prev = current;
		current = current->next;
	}

	if (NULL == current) {
		// we didn't find a block of memory that was large enough
		// we set the errno and return NULL
		errno = ENOMEM;
		return NULL;
	}

	// we remove the current block from the list
	if (prev) {
		prev->next = current->next;
	} else {
		head = current->next;
	}

	// we determine if the size of our current block is large enough to hold the user data as well as another mblock with size > 0
	const size_t block_has_rest = current->size > size + sizeof(struct mblock);

	if (block_has_rest) {
		// there is enough memory for another mblock

		// create and initialize the new mblock
		struct mblock *rest = (struct mblock *) (current->memory + size);
		rest->next = head;
		rest->size = current->size - size - sizeof(struct mblock);
		head = rest;

		// initialize the occupied mblock and return the pointer
		current->size = size;
		current->next = (struct mblock *) MAGIC;
		return current->memory;
	}


	// the rest-memory of our current block is not large enough to hold another mblock so we just give the user more memory
	// initialize the occupied mblock and return the pointer
	current->next = (struct mblock *) MAGIC;
	return current->memory;
}

void free(void *ptr) {
	if (NULL == ptr) {
		return;
	}

	// get access to the mblock
	// it startes 16 bytes (=sizeof(mblock)) before the pointer the user provides so we cast the void* to an mblock* and subtract 1 to go sizeof(mblock) bytes back
	// this works because the VLA memory in mblock has size 0
	struct mblock *block = ((struct mblock *) ptr) - 1;

	if (block->next != (struct mblock *) MAGIC) {
		// something wrote over the magic value -> this is a error created by the user
		abort();
	}

	block -> next = head;
	head = block;
}

void *realloc(void *ptr, size_t size) {
	if (size == 0) {
		// Ein realloc() auf Größe 0 soll sich dabei wie ein Aufruf von free() verhalten
		free(ptr);
		return NULL;
	}

	// allocate the new memory
	void *mem = malloc(size);
	if (NULL == mem) {
		// errno already set by malloc so we can just return
		return NULL;
	}

	if (NULL == ptr) {
		// Ein realloc() auf den Zeiger NULL soll sich dabei wie ein Aufruf an malloc() verhalten
		return mem;
	}

	// get the block for the memory provided by the user and determine if the new memory is smaller or larger
	struct mblock *block = ((struct mblock *) ptr) - 1;
	size_t min_size = block->size < size ? block->size : size;

	// copy the data
	// uses the smaller size
	memcpy(mem, ptr, min_size);

	// free the old memory
	free(ptr);

	return mem;
}

void *calloc(size_t nmemb, size_t size) {
	// allocate enough memory
	size_t total = nmemb * size;
	void *mem = malloc(total);
	if (NULL == mem) {
		// either size was zero or no memory was found
		// if no memory was found the errno is automatically set so there is nothing to do
		return NULL;
	}

	// initialize memory with 0
	memset(mem, 0, total);

	return mem;
}
