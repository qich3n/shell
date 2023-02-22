#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_STRING_LENGTH 255

typedef struct node {
    char *token;
    struct node *next;
} node;

node *tokenize(char *input);

int main(int argc, char **argv) {
    if (argc != 1) {
        fprintf(stderr, "Usage: %s < input.txt\n", argv[0]);
        exit(1);
    }

    char input[MAX_STRING_LENGTH];

    if (fgets(input, MAX_STRING_LENGTH, stdin) == NULL) {
        fprintf(stderr, "Error reading input\n");
        exit(1);
    }

    // Remove newline character from input, if present
    if (input[strlen(input) - 1] == '\n') {
        input[strlen(input) - 1] = '\0';
    }

    node *head = tokenize(input);
    node *current = head;

    while (current != NULL) {
        printf("%s\n", current->token);
        current = current->next;
    }

    // Free the memory used by the linked list
    current = head;
    while (current != NULL) {
        node *temp = current;
        current = current->next;
        free(temp->token);
        free(temp);
    }

    return 0;
}
node *tokenize(char *input) {
    node *head = NULL;
    node *tail = NULL;

    char *buffer = malloc(sizeof(char) * (MAX_STRING_LENGTH + 1));
    int buffer_index = 0;

    int in_quoted_string = 0;
    char quote_char;

    for (int i = 0; i < strlen(input); i++) {
        if (in_quoted_string) {
            if (input[i] == quote_char) {
                in_quoted_string = 0;

                buffer[buffer_index] = '\0';

                node *new_node = malloc(sizeof(node));
                new_node->token = malloc(sizeof(char) * (buffer_index + 1));
                strcpy(new_node->token, buffer);

                if (head == NULL) {
                    head = new_node;
                    tail = new_node;
                } else {
                    tail->next = new_node;
                    tail = new_node;
                }

                buffer_index = 0;
            } else {
                buffer[buffer_index] = input[i];
                buffer_index++;
            }
        } else if (input[i] == '"' || input[i] == '\'') {
            in_quoted_string = 1;
            quote_char = input[i];
        } else if (isspace(input[i]) || strchr("()<>;|!=", input[i]) != NULL) {
            if (buffer_index > 0) {
                buffer[buffer_index] = '\0';

                node *new_node = malloc(sizeof(node));
                new_node->token = malloc(sizeof(char) * (buffer_index + 1));
                strcpy(new_node->token, buffer);

                if (head == NULL) {
                    head = new_node;
                    tail = new_node;
                } else {
                    tail->next = new_node;
                    tail = new_node;
                }

                buffer_index = 0;
            }

            if (input[i] == '(' || input[i] == ')' || input[i] == '<' || input[i] == '>' || input[i] == ';' || input[i] == '|' || input[i] == '!') {
                char special_token[3];
                special_token[0] = input[i];
                special_token[1] = '\0';

                if (input[i] == '!' && i + 1 < strlen(input) && input[i + 1] == '=') {
                    special_token[1] = '=';
                    special_token[2] = '\0';
                    i++;
                }

                node *new_node = malloc(sizeof(node));
                new_node->token = malloc(sizeof(char) * 3);
                strcpy(new_node->token, special_token);

                if (head == NULL) {
                    head = new_node;
                    tail = new_node;
                } else {
                    tail->next = new_node;
                    tail = new_node;
                }
            }
        } else {
            buffer[buffer_index] = input[i];
            buffer_index++;
        }
    }

    if (buffer_index > 0) {
        buffer[buffer_index] = '\0';

        node *new_node = malloc(sizeof(node));
        new_node->token = malloc(sizeof(char) * (buffer_index + 1));
        strcpy(new_node->token, buffer);

        if (head == NULL) {
            head = new_node;
            tail = new_node;
        } else {
            tail->next = new_node;
            tail = new_node;
        }
    }

    free(buffer);
    tail->next = NULL;

    return head;
}
