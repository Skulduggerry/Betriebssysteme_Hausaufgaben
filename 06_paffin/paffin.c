#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>

#include "sem.h"
#include "list.h"

#define MAX_LINE 4096

// (module-)global variables
// static int activeGrepThreads;  // we don't need these variables
// static int activeCrawlThreads; // we don't need these variables
static SEM *crawlSem;
static SEM *grepSem;
static SEM *mutex;
static char *string;

// function declarations
static void *processTree(void *path);

static void *processDir(char *path);

static void *processEntry(char *path, struct dirent *entry);

static void *processFile(void *path);

// TODO: add declarations if necessary

static void usage(void) {
	fprintf(stderr, "Usage: paffin <string> <max-grep-threads> <trees...>\n");
	exit(EXIT_FAILURE);
}

static void die(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

/**
 * \brief Initializes necessary data structures and spawns one crawl-Thread per tree.
 * Subsequently, waits passively on the termination of all threads.
 * If all threads terminated, it dequeues all list entries, prints them, and subsequently
 * frees all allocated resources and exits/returns.
 */

int main(int argc, char **argv) {
	if (argc < 4) {
		usage();
	}

	// convert argv[2] (<max-grep-threads>) into long with strtol()
	errno = 0;
	char *endptr;
	int maxGrepThreads = strtol(argv[2], &endptr, 10);

	// argv[2] can not be converted into long without error
	if (errno != 0 || endptr == argv[2] || *endptr != '\0') {
		usage();
	}

	if (maxGrepThreads <= 0) {
		fprintf(stderr, "max-grep-threads must not be negative or zero\n");
		usage();
	}

	// OUR CODE

	// We set the initial value to 4 - argc, which is equal to -(argc - 3) + 1
	// Because of this after all argc-3 crawl threads are finished the semaphore is 1, and we continue the main
	// function after the P(crawlSem)
	crawlSem = semCreate(4 - argc);
	if (!crawlSem) {
		die("semCreate");
	}

	// a semaphore for the maximum number of grep threads
	grepSem = semCreate(maxGrepThreads);
	if (!grepSem) {
		die("semCreate");
	}

	// a mutex for the list
	mutex = semCreate(1);
	if (!mutex) {
		die("semCreate");
	}

	// we need to store the string globally to access for the grep threads
	string = argv[1];


	// start crawl threads
	for (int i = 3; i < argc; i++) {
		pthread_t thread;
		errno = pthread_create(&thread, NULL, processTree, argv[i]);
		if (errno) {
			die("pthread_create");
		}

		errno = pthread_detach(thread);
		if (errno) {
			die("pthread_detach");
		}
	}

	// wait for all crawl threads to finish
	P(crawlSem);

	// wait for all remaining grep threads to finish
	// nmo new grep threads are created because all crawl threads did already finish
	for (int i = 0; i < maxGrepThreads; i++) {
		P(grepSem);
	}

	// cleanup the semaphores
	semDestroy(mutex);
	semDestroy(grepSem);
	semDestroy(crawlSem);

	// print out all strings
	char *res;
	while ((res = dequeue()) != NULL) {
		if (fprintf(stdout, "%s", res) < 0) {
			die("fprintf");
		}
		free(res);
	}

	// flush the stream
	if (fflush(stdout)) {
		die("fflush");
	}

	return EXIT_SUCCESS;
}

/**
 * \brief Acts as start_routine for crawl-Threads and calls processDir().
 *
 * \param path Path to the directory to process
 *
 * \return Always returns NULL
 */
static void *processTree(void *path) {
	processDir(path);

	// we are finished with processing the tree so we increase the crawlSem by 1
	V(crawlSem);

	return NULL;
}

/**
 * \brief Iterates over all directory entries of path and calls processEntry()
 * on each entry (except "." and "..").
 *
 * \param path Path to directory to process
 *
 * \return Always returns NULL
 */

static void *processDir(char *path) {
	DIR *dir = opendir(path);
	if (dir == NULL) {
		// also occurs when the user inputs a file instead of a directory as a tree
		die("opendir");
	}

	errno = 0; // required to distinguish errors from end of directory

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			// we skip '.' and '..'
			continue;
		}
		processEntry(path, entry);
	}

	if (errno) {
		die("readdir");
	}

	if (closedir(dir)) {
		die("closedir");
	}

	return NULL;
}

/**
 * \brief Spawns a new grep-Thread if the entry is a regular file and calls processDir() if the entry
 * is a directory.
 *
 * It updates activeGrepThreads if necessary. If the maximum number of active grep-Threads is
 * reached the functions waits passively until another grep-Threads can be spawned.
 *
 * \param path Path to the directory of the entry
 * \param entry Pointer to struct dirent as returned by readdir() of the entry
 *
 * \return Always return NULL
 */
static void *processEntry(char *path, struct dirent *entry) {
	// concatenate path and entry filename
	int pathLen = strlen(path);
	int nameLen = strlen(entry->d_name);
	char *newPath = malloc(pathLen + nameLen + 2); // dir path + filename + / + \0
	if (newPath == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	memcpy(newPath, path, pathLen);
	newPath[pathLen] = '/';
	memcpy(newPath + pathLen + 1, entry->d_name, nameLen);
	newPath[pathLen + 1 + nameLen] = '\0';

	// request file information
	struct stat buf;
	if (lstat(newPath, &buf) == -1) {
		perror("lstat");
		exit(EXIT_FAILURE);
	}

	if (S_ISDIR(buf.st_mode)) {
		processDir(newPath);
		free(newPath);
	} else if (S_ISREG(buf.st_mode)) {
		P(grepSem); // wait if no new grep thread can be created

		pthread_t thread;
		errno = pthread_create(&thread, NULL, processFile, newPath);
		if (errno) {
			die("pthread_create");
		}

		errno = pthread_detach(thread);
		if (errno) {
			die("pthread_detach");
		}
	}

	return NULL;
}

/**
 * \brief Acts as start_routine for grep-Threads and searches all lines of the file for the
 * search pattern.
 *
 * It adds a line, the corresponding line number and the path to the file to the list if the
 * search pattern is found.
 *
 * \param path Path to the file to process
 *
 * \return Always returns NULL
 */
static void *processFile(void *path) {
	FILE *file = fopen(path, "r");
	if (file == NULL) {
		die("fopen");
	}

	char line[MAX_LINE + 2];
	int lineNum = 1;
	while (fgets(line, MAX_LINE + 2, file) != NULL) {
		if (strstr(line, string) != NULL) {
			P(mutex);
			if(enqueue(path, line, lineNum)) {
				die("enqueue");
			}
			V(mutex);
		}
		lineNum++;
	}

	// we exited the loop because fgets returned NULL
	// we need to check why
	if (ferror(stdin)) {
		// an error occurred and we quit the program
		die("fgets");
	}

	// close the file and free the memory
	if (fclose(file)) {
		die("fclose");
	}
	free(path);

	// we are finished with processing the tree so we increase the grepSem by 1
	V(grepSem);

	return NULL;
}

