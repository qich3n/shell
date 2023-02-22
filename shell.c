#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_COMMAND_LENGTH 255
#define MAX_ARGS 64
#define MAX_OUTPUT_LENGTH 4096

int is_exit_command(const char *command) {
    return strcmp(command, "exit") == 0;
}

int run_command(const char *command, char *output, int output_size) {
    char *args[MAX_ARGS];
    char *token;
    int num_args;
    int status;
    int result = 0;

    num_args = 0;
    token = strtok((char *) command, " ");
    while (token != NULL) {
        if (strlen(token) > 0) {
            // check for quotes at beginning of argument
            if (token[0] == '"' || token[0] == '\'') {
                if (token[strlen(token) - 1] == token[0]) {
                    // argument is fully enclosed in quotes
                    token++;
                    token[strlen(token) - 1] = '\0';
                } else {
                    // argument starts with quotes but does not end with them
                    // continue parsing quoted argument
                    int arg_len = strlen(token);
                    for (int i = 1; i < strlen(token); i++) {
                        if (token[i] == token[0]) {
                            arg_len = i;
                            break;
                        }
                    }
                    char *arg = malloc(arg_len);
                    strncpy(arg, token + 1, arg_len - 1);
                    arg[arg_len - 1] = '\0';
                    args[num_args] = arg;
                    num_args++;
                    continue;
                }
            }

            int arg_len = strlen(token);
            char *arg = malloc(arg_len + 1);
            strncpy(arg, token, arg_len);
            arg[arg_len] = '\0';
            args[num_args] = arg;
            num_args++;
        }
        token = strtok(NULL, " ");
    }
    args[num_args] = NULL;

    if (num_args > 0) {
        pid_t pid = fork();
        if (pid == 0) {
            // child process
            if (execvp(args[0], args) == -1) {
                perror(args[0]);
                exit(1);
            }
        } else if (pid == -1) {
            perror("fork");
            result = 1;
        } else {
            // parent process
            if (waitpid(pid, &status, 0) == -1) {
                perror("waitpid");
                result = 1;
            }
        }

        // free memory allocated for arguments
        for (int i = 0; i < num_args; i++) {
            free(args[i]);
        }
    }

    return result;
}
int run_commands(const char *commands, char *output, int output_size) {
    char command[MAX_COMMAND_LENGTH];
    int num_commands = 1;
    const char *p = commands;

    // count the number of commands
    while (*p != '\0') {
        if (*p == ';') {
            num_commands++;
        }
        p++;
    }

    // allocate an array to hold the commands
    char *command_list[num_commands];

    // split the commands into the array
    int i = 0;
    p = commands;
    while (*p != '\0' && i < num_commands) {
        int j = 0;
        int in_quotes = 0;
        while (*p != '\0' && (*p != ';' || in_quotes)) {
            if (*p == '"' || *p == '\'') {
                in_quotes = !in_quotes;
            }
            command[j] = *p;
            j++;
            p++;
        }
        command[j] = '\0';
        command_list[i] = malloc(j + 1);
        strncpy(command_list[i], command, j + 1);
        i++;
        if (*p == ';') {
            p++;
        }
    }

    // process each command in sequence
    int result = 0;
    output[0] = '\0';
    for (int i = 0; i < num_commands; i++) {
        // run the current command and append its output to the result
        char command_output[MAX_OUTPUT_LENGTH];
        int command_status = run_command(command_list[i], command_output, MAX_OUTPUT_LENGTH);
        strncat(output, command_output, output_size - strlen(output) - 1);
        if (command_status != 0) {
            result = command_status;
            break;
        }

        // free the memory allocated for the current command
        free(command_list[i]);
    }

    return WIFEXITED(result) ? WEXITSTATUS(result) : -1;
}

int main(int argc, char **argv) {
    printf("Welcome to mini-shell.\n");
    char command[MAX_COMMAND_LENGTH];
    char command_output[MAX_OUTPUT_LENGTH];
    int status;

    while (1) {
        printf("shell $ ");  // display prompt
        if (fgets(command, MAX_COMMAND_LENGTH, stdin) == NULL) {
            if (feof(stdin)) {  // end-of-file detected
                printf("\nBye bye.\n");
                break;
            } else {
                perror("fgets");
                break;
            }
        }
        command[strcspn(command, "\n")] = '\0';  // remove newline from input

        if (is_exit_command(command)) {
            printf("Bye bye.\n");
            break;
        }

        int result = run_commands(command, command_output, MAX_OUTPUT_LENGTH);
        if (result != 0) {
            printf("Command failed with status %d.\n", result);
        } else {
            printf("%s", command_output);
        }
    }

    return 0;
}
