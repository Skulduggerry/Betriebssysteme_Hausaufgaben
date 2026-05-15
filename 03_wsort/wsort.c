#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct String {
    char *data;
    int len; // the length of the string without the \0
    int capacity; // the total capacity of the string
} string_t;

typedef struct StringArray {
    string_t *data;
    int len;
    int capacity;
} string_array_t;

// initialize a string with a given capacity
// does nothing if the string is already initialized
void string_init(string_t *string, int capacity) {
    if (string->data != NULL) {
        return;
    }

    string->len = 0;
    string->capacity = capacity;
    string->data = malloc(sizeof(char) * capacity);
    if (NULL == string->data) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
}

// grow the buffer size of a string by a factor of 2
// does nothing if the string is uninitialized
void string_grow(string_t *string) {
    if (NULL == string->data) {
        return;
    }

    string->data = realloc(string->data, string->capacity * 2 * sizeof(char));
    if (NULL == string->data) {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    string->capacity *= 2;
}

void string_free(string_t *string) {
    free(string->data);
    string->data = NULL;
    string->len = 0;
    string->capacity = 0;
}

// initialize a string array with a given capacity
// does nothing if the array is already initialized
void string_array_init(string_array_t *array, int capacity) {
    if (array->data != NULL) {
        return;
    }

    array->len = 0;
    array->capacity = capacity;
    array->data = malloc(sizeof(string_t) * capacity);
    if (NULL == array->data) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
}

// grow the buffer size of a string array by a factor of 2
// does nothing if the array is uninitialized
void string_array_grow(string_array_t *array) {
    if (NULL == array->data) {
        return;
    }

    array->data = realloc(array->data, array->capacity * 2 * sizeof(string_t));
    if (NULL == array->data) {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    array->capacity *= 2;
}

// the string array takes ownership of the string
// this means it steal all attributes from string and puts it into an uninitialized state
void string_array_add(string_array_t *array, string_t *string) {
    if (NULL == array->data || NULL == string->data) {
        return;
    }

    // grow the array if it is full
    // it should never happen that len > capacity
    // for simplicity and since I am the only one ever touching this code I will omit error handling
    if (array->len == array->capacity) {
        string_array_grow(array);
    }

    // copy all data from string into the array
    array->data[array->len].data = string->data;
    array->data[array->len].len = string->len;
    array->data[array->len].capacity = string->capacity;
    array->len++;

    // put the string into an uninitialized state
    string->data = NULL;
    string->len = 0;
    string->capacity = 0;
}

void string_array_free(string_array_t *array) {
    // first free all strings
    for (int i = 0; i < array->len; i++) {
        string_free(&array->data[i]);
    }

    // the free the
    free(array->data);
    array->len = 0;
    array->capacity = 0;
}

// returns the word length of a string
// that means the number of characters excluding \n and \0
// an uninitialized string return -1
int string_word_length(string_t *string) {
    if (NULL == string->data) {
        return -1;
    }
    return string->data[string->len - 1] == '\n' ? string->len - 1 : string->len;
}

// removes a \n at the end of a string
// does nothing if string is uninitialized
void string_remove_newline(string_t *string) {
    if (NULL == string->data) {
        return;
    }

    // set the first character after the word to '\0'
    // if there was no \n nothing happens
    // otherwise the '\n' is replaced
    int word_length = string_word_length(string);
    string->data[word_length] = '\0';

    // in case there was a '\n' we have to shorten the string size by 1 because we deleted one character from the string
    if (string->len != word_length) {
        string->len -= 1;
    }
}

// this function reads a full line as a string from stdin
// the function returns the string that was given in or NULL if we reached EOF
// the string that is passed in might be uninitialized in which case it will be initialized by the function
// any previous content of the string will be overridden
string_t *read_line(string_t *string) {
    // initialize the string or set it's sice to zero to override previous content
    string->len = 0;
    string_init(string, 16);

    while (1) {
        // we start writing into the start at offset len
        // this overrides the \0 of previous iterations since the length of the string exclude the \0 while preserving all other content
        char *start = string->data + string->len;

        // we now read into the string
        // the amount of space we have left is the capacity minus the length of the string
        char *result = fgets(start, string->capacity - string->len, stdin);

        // we check the result of fgets
        if (NULL == result) {
            if (feof(stdin)) {
                // we reached EOF so we can stop reading
                break;
            }

            if (ferror(stdin)) {
                // an error occurred and we quit the program
                perror("fgets");
                exit(EXIT_FAILURE);
            }
        }

        // the new string length is the old string length plus the amount of characters that we read
        string->len += strlen(start);

        // if the string was not fully filled which means that capacity and len differ be more then 1 (remember len
        // excludes the \0 while capacity does not) we reached an end in the form if EOF or a new line
        // in case they differ by exactly one we check the last character of it was a \n which means that we also stop reading
        if (string->capacity - string->len > 1 || string->data[string->len - 1] == '\n') {
            break;
        }

        // the string capacity is fully filled up so we grow the string size and repeat the loop
        string_grow(string);
    }

    // if the string length was 0 we read nothing so we return NULL to indicate that we reached EOF
    // otherwise we return the string to indicate that we read something
    if (string->len == 0) {
        string_free(string); // we free the string as we did not read anything
        return NULL;
    }
    return string;
}

// this functions reads all lines from stdin
// it does not contain any empty lines or lines with a word sice of more than 100
// the array that is passed in might be uninitialized in which case it will be initialized by the function
// any previous content of the array will be overridden
void read_file(string_array_t *array) {
    // initialize the string or set it's sice to zero to override previous content
    array->len = 0;
    string_array_init(array, 16);

    while (1) {
        string_t string = {.data = NULL, .len = 0, .capacity = 0};
        string_t *result = read_line(&string);
        if (NULL == result) {
            return;
        }

        string_remove_newline(&string);

        if (string.len == 0) {
            string_free(&string);
            continue;
        }

        if (string.len > 100) {
            string_free(&string);
            if (fprintf(stderr, "Wort zu lang...ignoriere\n") < 0) {
                perror("fprintf");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        string_array_add(array, &string);
    }
}

int cmp(const void *a, const void *b) {
    const string_t *x = a;
    const string_t *y = b;
    return strcmp(x->data, y->data);
}

int main(void) {
    // read all lines from stdin and sort them
    string_array_t array = {.data = NULL, .len = 0, .capacity = 0};
    read_file(&array);

    qsort(array.data, array.len, sizeof(string_t), cmp);

    // print out all strings in sorted order
    for (int i = 0; i < array.len; i++) {
        if (printf("%s\n", array.data[i].data) < 0) {
            perror("printf");
            exit(EXIT_FAILURE);
        }
    }

    if (EOF == fflush(stdout)) {
        perror("fflush");
        exit(EXIT_FAILURE);
    }

    string_array_free(&array);

    return EXIT_SUCCESS;
}
