#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct StringArray {
    char **data;
    int len;
    int capacity;
} string_array_t;

#define STRING_BUFFER_LEN 102 // the maximum length of a word + \n + \0

// grows the array size by a factor of 2 if it is full
void grow_if_needed(string_array_t *array) {
    if (array->len > array->capacity) {
        // if we reach this we did something horribly wrong
        exit(EXIT_FAILURE);
    }

    if (array->len != array->capacity) {
        // no growing is required since len < capacity
        return;
    }

    // grow the array size by a factor of 2 since it is full
    char **tmp = realloc(array->data, array->capacity * 2 * sizeof(char *));
    if (NULL == tmp) {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    array->data = tmp;
    array->capacity *= 2;
}

string_array_t read_file(void) {
    // create an array with enough space for 16 strings
    string_array_t array = {.data = NULL, .len = 0, .capacity = 16};
    array.data = malloc(16 * sizeof(char *));
    if (NULL == array.data) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // read all strings from stdin
    while (1) {
        // create space for the word
        char *word_start = malloc(STRING_BUFFER_LEN * sizeof(char));
        if (NULL == word_start) {
            perror("malloc");
            exit(1);
        }

        // read the word
        if (NULL == fgets(word_start, STRING_BUFFER_LEN, stdin)) {
            if (feof(stdin)) {
                // nothing was read because we reached the end
                // we can free the memory for the string and break our loop
                free(word_start);
                break;
            }

            if (ferror(stdin)) {
                // an error occurred during the read
                // we print an error message and exit (no error handling for the error handling)
                fprintf(stderr, "Could not read from stdin!");
                exit(EXIT_FAILURE);
            }
        }

        // we calculate the word length
        // if the word ended with '\n' the length is the distance between word_start and the '\n'
        // otherwise the word is not finished or reading stopped because of EOF and we calculate the length with strlen
        char *word_end = strchr(word_start, '\n');
        int word_len = word_end != NULL ? word_end - word_start : strlen(word_start);

        // if the word has length 0 (empty line) we can free the memory and continue reading in the next line
        if (word_len == 0) {
            free(word_start);
            continue;
        }

        // if the word length was more than 100 we read until we reached the end of the line
        if (word_len > 100) {
            // print message for the user
            fprintf(stderr, "Wort zu lang...ignoriere\n");

            // read until we reach the end of line
            while (fgets(word_start, STRING_BUFFER_LEN, stdin) != NULL) {
                // check if we reached the end of the line and quit
                if (strchr(word_start, '\n') != NULL) {
                    break;
                }
            }

            // we ended because of EOF so we free the string (it was too long) and can stop reading so we quit the loop
            if (feof(stdin)) {
                free(word_start);
                break;
            }

            // an error occurred during the read
            // we print an error message and exit (no error handling for the error handling)
            if (ferror(stdin)) {
                fprintf(stderr, "Could not read from stdin!");
                exit(EXIT_FAILURE);
            }

            // we reached the end of the word without a problem
            // we free the buffer since the word was to long and continue with the next line
            free(word_start);
            continue;
        }

        // we found a valid word and now have to add it
        if (NULL != word_end) {
            // the word ended with a '\n'
            // we replace this with a '\0' to not have two new lines in our output
            *word_end = '\0';
        }

        // we grow the array if needed and add the string to its entries
        grow_if_needed(&array);
        array.data[array.len++] = word_start;
    }

    return array;
}

int cmp(const void *a, const void *b) {
    const char **x = (const char **) a;
    const char **y = (const char **) b;
    return strcmp(*x, *y);
}

int main(void) {
    // read all lines from stdin and sort them
    string_array_t array = read_file();
    qsort(array.data, array.len, sizeof(char *), cmp);

    // print out all strings in sorted order
    for (int i = 0; i < array.len; i++) {
        if (printf("%s\n", array.data[i]) < 0) {
            perror("printf");
            exit(EXIT_FAILURE);
        }
    }

    if(EOF == fflush(stdout)) {
        perror("fflush");
        exit(EXIT_FAILURE);
    }

    // we first free all strings and then the string array
    for (int i = 0; i < array.len; i++) {
        free(array.data[i]);
    }
    free(array.data);

    return EXIT_SUCCESS;
}
