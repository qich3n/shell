#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>
#include <fcntl.h>
#define MAX_COMMAND_LENGTH 255
#define MAX_ARGS 64
#define MAX_OUTPUT_LENGTH 4096

int is_exit_command(const char *command)
{
	return strcmp(command, "exit") == 0;
}

int run_command(const char *command, char *output, int output_size)
{
	char *args[MAX_ARGS];
	char *token;
	int num_args;
	int status;
	int result = 0;

	num_args = 0;
	int in_quotes = 0;	// flag to track if currently inside quotes
	int redirect_in = 0;	// flag to track if input redirection is occurring
	int redirect_out = 0;	// flag to track if output redirection is occurring
	char *arg = NULL;	// pointer to the current argument being built
	const char *p = command;
	while (*p != '\0')
	{
		char c = *p;
		if (c == ' ' && !in_quotes)
		{
			// end of an argument
			if (arg != NULL)
			{
				args[num_args] = arg;
				num_args++;
				arg = NULL;
			}
		}
		else if (c == '<' && !in_quotes)
		{
			redirect_in = !redirect_in;
		}
		else if (c == '>' && !in_quotes)
		{
			redirect_out = !redirect_out;
		}
		else if (c == '"' || c == '\'')
		{
			// toggle in_quotes flag
			if (in_quotes && arg != NULL)
			{
				args[num_args] = arg;
				num_args++;
				arg = NULL;
			}

			in_quotes = !in_quotes;
		}
		else
		{
			// add character to current argument
			if (arg == NULL)
			{
				arg = malloc(MAX_COMMAND_LENGTH);
				arg[0] = '\0';
			}

			int arg_len = strlen(arg);
			if (arg_len < MAX_COMMAND_LENGTH - 1)
			{
				arg[arg_len] = c;
				arg[arg_len + 1] = '\0';
			}
		}

		p++;
	}

	if (arg != NULL)
	{
		args[num_args] = arg;
		num_args++;
	}

	if (num_args > 0)
	{
		args[num_args] = NULL;

		if (strcmp(args[0], "cd") == 0)
		{
			// change the working directory
			if (num_args == 1)
			{
			 	// no argument provided, change to home directory
				const char *home = getenv("HOME");
				if (home == NULL)
				{
					home = "/";
				}

				if (chdir(home) != 0)
				{
					perror("chdir");
					result = 1;
				}
			}
			else if (chdir(args[1]) != 0)
			{
			 	// change to the directory provided as the first argument
				perror(args[1]);
				result = 1;
			}
		}
		else
		{
			pid_t pid = fork();
			if (pid == 0)
			{
			 	// child process
				if (redirect_in)
				{
					if (close(0) == -1)
					{
						perror("Error closing stdin");
						exit(1);
					}

					int fd = open(args[num_args - 1], O_RDONLY);
					args[num_args - 1] = NULL;
					assert(fd == 0);
				}
				else if (redirect_out)
				{
					if (close(1) == -1)
					{
						perror("Error closing stdout");
						exit(1);
					}

					int fd = open(args[num_args - 1], O_WRONLY | O_TRUNC | O_CREAT, 0644);
					args[num_args - 1] = NULL;
					assert(fd == 1);
				}

				if (execvp(args[0], args) == -1)
				{
					perror(args[0]);
					exit(1);
				}
			}
			else if (pid == -1)
			{
				perror("fork");
				result = 1;
			}
			else
			{
			 	// parent process
				if (waitpid(pid, &status, 0) == -1)
				{
					perror("waitpid");
					result = 1;
				}
			}
		}

		// free memory allocated for arguments
		for (int i = 0; i < num_args; i++)
		{
			free(args[i]);
		}
	}

	return result;
}

int run_commands(const char *commands, char *output, int output_size)
{
	char command[MAX_COMMAND_LENGTH];
	int num_commands = 1;
	const char *p = commands;

	// count the number of commands
	while (*p != '\0')
	{
		if (*p == ';')
		{
			num_commands++;
		}

		p++;
	}

	// allocate an array to hold the commands
	char *command_list[num_commands];

	// split the commands into the array
	int i = 0;
	p = commands;
	while (*p != '\0' && i < num_commands)
	{
		int j = 0;
		int in_quotes = 0;
		while (*p != '\0' && (*p != ';' || in_quotes))
		{
			if (*p == '"' || *p == '\'')
			{
				in_quotes = !in_quotes;
			}

			command[j] = *p;
			j++;
			p++;
		}

		command[j] = '\0';
		if (j > 0)
		{
			// only add non-empty commands to the list
			command_list[i] = malloc(j + 1);
			strncpy(command_list[i], command, j + 1);
			i++;
		}

		if (*p == ';')
		{
			p++;
		}
	}

	num_commands = i;	// update the number of commands in the list

	// process each command in sequence
	int result = 0;
	output[0] = '\0';
	for (int i = 0; i < num_commands; i++)
	{
		// run the current command and append its output to the result
		char command_output[MAX_OUTPUT_LENGTH];
		int command_status = run_command(command_list[i], command_output, MAX_OUTPUT_LENGTH);
		if (command_output[0] == '\'')
		{
			// Output string without single quotes
			strncat(output, " ", output_size - strlen(output) - 1);
			strncat(output, command_output + 1, output_size - strlen(output) - 1);
			output[strlen(output) - 1] = '\0';	// Remove the last quote
		}
		else
		{
			strncat(output, command_output, output_size - strlen(output) - 1);
		}

		if (command_status != 0)
		{
			result = command_status;
			break;
		}

		// free the memory allocated for the current command
		free(command_list[i]);
	}

	return WIFEXITED(result) ? WEXITSTATUS(result) : -1;
}

int main(int argc, char **argv)
{
	printf("Welcome to mini-shell.\n");
	char command[MAX_COMMAND_LENGTH];
	char command_output[MAX_OUTPUT_LENGTH];
	int status;
	char prev_command[MAX_COMMAND_LENGTH] = "";	// new line to remember previous command

	while (1)
	{
		printf("shell $ ");	// display prompt
		if (fgets(command, MAX_COMMAND_LENGTH, stdin) == NULL)
		{
			if (feof(stdin))
			{
				// end-of-file detected
				printf("\nBye bye.\n");
				break;
			}
			else
			{
				perror("fgets");
				break;
			}
		}

		command[strcspn(command, "\n")] = '\0';	// remove newline from input

		if (is_exit_command(command))
		{
			printf("Bye bye.\n");
			break;
		}

		if (strcmp(command, "prev") == 0)
		{
			// new condition to execute previous command
			if (strlen(prev_command) == 0)
			{
				printf("No previous command.\n");
			}
			else
			{
				strncpy(command, prev_command, MAX_COMMAND_LENGTH);
				printf("Running previous command: %s\n", command);
			}
		}
		else
		{
			// remember the current command as the previous command
			strncpy(prev_command, command, MAX_COMMAND_LENGTH);

			// run the command and get its return status
			int result = run_commands(command, command_output, MAX_OUTPUT_LENGTH);

			// print the output of the command
			int command_output_len = strlen(command_output);
			if (command_output_len > 0)
			{
			 	// remove any trailing newline characters from the command output
				if (command_output[command_output_len - 1] == '\n')
				{
					command_output[command_output_len - 1] = '\0';
				}

				printf("%s\n", command_output);
			}

			// clear the command output buffer
			command_output[0] = '\0';
		}
	}

	return 0;
}
