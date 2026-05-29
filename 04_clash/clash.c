#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>

#include "plist.h"

// ==========================================
// WE COPIED CODE FROM OUR wsort TO SAVE TIME
// ==========================================

typedef struct String {
    char *data;
    int len; // the length of the string without the \0
    int capacity; // the total capacity of the string
} string_t;

typedef struct StringArray {
    char **data;
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

// removes a \n at the end of a string
// does nothing if string is uninitialized
void string_remove_newline(string_t *string) {
    if (NULL == string->data) {
        return;
    }

    if (string->data[string->len - 1] == '\n') {
        string->data[string->len - 1] = '\0';
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

// initialize a string array with a given capacity
// does nothing if the array is already initialized
void string_array_init(string_array_t *array, int capacity) {
    if (array->data != NULL) {
        return;
    }

    array->len = 0;
    array->capacity = capacity;
    array->data = malloc(sizeof(char *) * capacity);
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

    array->data = realloc(array->data, array->capacity * 2 * sizeof(char *));
    if (NULL == array->data) {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    array->capacity *= 2;
}

// copies the string into the array
// does nothing if the array is not initialized
void string_array_add(string_array_t *array, const char *string) {
    if (NULL == array->data) {
        return;
    }

    // grow the array if it is full
    // it should never happen that len > capacity
    // for simplicity and since I am the only one ever touching this code I will omit error handling
    if (array->len == array->capacity) {
        string_array_grow(array);
    }


    // if the string is null we cannot make a copy but instead just write NULL into the array
    if (string == NULL) {
        array->data[array->len++] = NULL;
        return;
    }

    // create a copy of the string
    char *copy = strdup(string);
    if (NULL == copy) {
        // we had not enough memory to copy the string
        perror("strdup");
        exit(EXIT_FAILURE);
    }

    // put the copy into the array
    array->data[array->len++] = copy;
}

void string_array_free(string_array_t *array) {
    // first free all strings
    for (int i = 0; i < array->len; i++) {
        free(array->data[i]);
    }

    // free the array
    free(array->data);
    array->len = 0;
    array->capacity = 0;
}

// ====================
// THIS IS OUR NEW CODE
// ====================

void our_getcwd(string_t *string) {
    // initialize the string or set it's sice to zero to override previous content
    string->len = 0;
    string_init(string, 1024);

    // try to READ the CWD
    // if the function returns a value other than NULL this indicates successful reading
    while (NULL == getcwd(string->data, string->capacity)) {
        if (errno != ERANGE) {
            // we failed for a reason other than a to small buffer
            // this is an error we cannot handle
            perror("getcwd");
            exit(EXIT_FAILURE);
        }

        // in case reading failed because the buffer was to small we increase the buffer size
        string_grow(string);
    }

    // we read the CWD successfully
    // we just set the len of the string to the correct value
    string->len = strlen(string->data);
}

// this function tokenizes a string into a parameter list for a program
// the array that is passed in might be uninitialized in which case it will be initialized by the function
// any previous content of the array will be overridden
// the array will be terminated by a NULL pointer
void tokenize_into_array(string_array_t *array, string_t *string) {
    array->len = 0;
    string_array_init(array, 16);

    char *copy = malloc(sizeof(char) * (string->len + 1));
    if (NULL == copy) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    strcpy(copy, string->data);

    char *current = strtok(copy, "\t ");

    // tokenize until there are no more tokens
    while (NULL != current) {
        string_array_add(array, current);
        current = strtok(NULL, "\t ");
    }

    // we add a NULL pointer to mark the end of the argument list
    string_array_add(array, NULL);

    // we free the copy as it is no longer used
    free(copy);
}

bool printJob(pid_t pid, const char *cmdLine) {
    if (printf("%d %s\n", pid, cmdLine) < 0) {
        perror("printf");
        exit(EXIT_FAILURE);
    }

    return false;
}

int main(void) {
    while (1) {
        // read and print the CWD
        string_t cwd = {
            .data = NULL, .len = 0, .capacity = 0
        };
        our_getcwd(&cwd);
        if (printf("%s: ", cwd.data) < 0) {
            perror("printf");
            exit(EXIT_FAILURE);
        }
        string_free(&cwd);

        // read the line inputted by the user
        string_t user_input = {.data = NULL, .len = 0, .capacity = 0};
        if (NULL == read_line(&user_input)) {
            // we reached EOF (indicated by a return value of NULL) because the user used CTRL+D
            // we quit our program
            break;
        }

        if (user_input.len > 1337) {
            if (printf("Input is too long\n") < 0) {
                perror("printf");
                exit(EXIT_FAILURE);
            }
            string_free(&user_input);
            continue;
        }

        string_remove_newline(&user_input); // remove the newline that is at the end of the console input

        // split the user input into tokens
        string_array_t array = {.data = NULL, .len = 0, .capacity = 0};
        tokenize_into_array(&array, &user_input);

        if (array.len == 1) {
            // if nothing was entered we just restart the loop
            // the len is then 1 because we terminate the array with a NULL pointer
            string_free(&user_input);
            string_array_free(&array);
            continue;
        }

        // implement jobs
        if (strcmp(array.data[0], "jobs") == 0) {
            if (array.len != 2) {
                // array is not: jobs + NULL
                if (printf("usage: cd <directory>\n") < 0) {
                    perror("printf");
                    exit(EXIT_FAILURE);
                }
            } else {
                walkList(printJob);
            }

            string_free(&user_input);
            string_array_free(&array);
            continue;
        }

        // implement cd <directory>
        if (strcmp(array.data[0], "cd") == 0) {
            if (array.len != 3) {
                // array is not: cd +  <directory> + NULL
                if (printf("usage: cd <directory>\n") < 0) {
                    perror("printf");
                    exit(EXIT_FAILURE);
                }
            } else {
                chdir(array.data[1]);
            }
            string_free(&user_input);
            string_array_free(&array);
            continue;
        }

        int exec_in_background = array.len > 1 && strcmp(array.data[array.len - 2], "&") == 0;
        if (exec_in_background) {
            // if we figured out that we will execute in background we remove the "&" from the programs argument list
            free(array.data[array.len - 2]); // free the bytes of the "&"
            array.data[array.len - 2] = NULL; //
            array.len -= 1;
        }

        int pid = fork();
        if (-1 == pid) {
            // an error occurred so we quit the program
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            // child process
            execvp(array.data[0], array.data);
            perror("execvp");
            exit(EXIT_FAILURE);
        }

        // when we are in the parent process
        // the child will never reach this code because of execvp (or exit of execvp fails)

        if (exec_in_background) {
            // we want to execute in background so we put the child into the queue
            insertElement(pid, user_input.data);
        } else {
            int status;
            if (waitpid(pid, &status, 0) && WIFEXITED(status)) {
                if (printf("Exitstatus [%s] = %d\n", user_input.data, WEXITSTATUS(status)) < 0) {
                    perror("printf");
                    exit(EXIT_FAILURE);
                }
            }
        }

        int status;
        int wpid;
        while ((wpid = waitpid(-1, &status, WNOHANG)) > 0) {
            // we have a child process that we waited for

            // read the command line argument of the child into our buffer
            string_t childCmd = {.data = NULL, .len = 0, .capacity = 0};
            string_init(&childCmd, 1337);
            if (removeElement(wpid, childCmd.data, childCmd.capacity) == -1) {
                printf("Failed to remove child");
            }

            // print the exit status
            if (printf("Exitstatus [%s] = %d\n", childCmd.data, WEXITSTATUS(status)) < 0) {
                perror("printf");
                exit(EXIT_FAILURE);
            }

            // free the buffer
            string_free(&childCmd);
        }

        // find out the reason why the loop ended
        if (wpid == -1 && errno != ECHILD) {
            // waitpid did not just fail because we did notr have a child
            perror("waitpid");
            exit(EXIT_FAILURE);
        }

        // free the user input and string array
        string_free(&user_input);
        string_array_free(&array);
    }

    return EXIT_SUCCESS;
}
