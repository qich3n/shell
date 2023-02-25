#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#define MAX_COMMAND_LENGTH 255
#define MAX_ARGS 64
#define MAX_OUTPUT_LENGTH 4096

// is a function that takes a file pointer as input and returns the file descriptor associated with that file pointer.
int fileno(FILE*);

// Global variables to store the previous command
char previous_command[MAX_COMMAND_LENGTH];
char previous_output[MAX_OUTPUT_LENGTH];

// display help text
void
display_help()
{
	printf("List of built-in commands:\n");
	printf("  cd<directory>: change the working directory\n");
	printf("  source<file>: execute commands from a file\n");
	printf("  prev: print the previous command and its output\n");
	printf("  help: display this help text\n");
	printf("  exit: exit the shell\n");
}

int
is_exit_command(const char *command)
{
	return strcmp(command, "exit") == 0;
}

int run_command(const char *command, char *output, int output_size)
{
	static char previous_command[MAX_COMMAND_LENGTH] = "";
	static char previous_output[MAX_OUTPUT_LENGTH] = "";

	int is_background = 0;
	char *args[MAX_ARGS];
	char *args2[MAX_ARGS];
	int num_args = 0;
	int result = 0;
	int in_quotes = 0;
	char *arg = NULL;
	const char *p = command;
	int status;
	int is_pipeline = 0;
	int pipefd[2];
	const char *pipe_token = "|";
	char *token_pos = strstr(command, pipe_token);
	if (token_pos != NULL)
	{
		is_pipeline = 1;
	}

	while (*p != '\0')
	{
		char c = *p;
		if (c == ' ' && !in_quotes)
		{
			if (arg != NULL)
			{
				args[num_args] = arg;
				num_args++;
				arg = NULL;
			}
		}
		else if (c == '"' || c == '\'')
		{
			if (in_quotes && arg != NULL)
			{
				args[num_args] = arg;
				num_args++;
				arg = NULL;
			}

			in_quotes = !in_quotes;
		}
		else if (c == '<' && !in_quotes)
		{
			if (arg != NULL)
			{
				args[num_args] = arg;
				num_args++;
				arg = NULL;
			}

			// get the filename from the command string
			char filename[MAX_COMMAND_LENGTH];
			p++;
			int i = 0;
			while (*p != '\0' && *p != ' ' && *p != '"' && *p != '\'')
			{
				filename[i] = *p;
				i++;
				p++;
			}

			filename[i] = '\0';
			// open the file and redirect input
			FILE *infile = fopen(filename, "r");
			if (infile == NULL)
			{
				perror(filename);
				result = 1;
				break;
			}

			if (dup2(fileno(infile), STDIN_FILENO) == -1)
			{
				perror("dup2");
				result = 1;
				break;
			}

			fclose(infile);
		}
		else
		{
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
		args[num_args] = NULL;

	if (strcmp(args[0], "cd") == 0)
	{
		if (num_args == 1)
		{
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
			perror(args[1]);
			result = 1;
		}
	}
	else if (strcmp(args[0], "source") == 0)
	{
		// execute commands from a file
		if (num_args == 1)
		{
			// no file name provided
			fprintf(stderr, "Usage: source<file>\n");
			result = 1;
		}
		else
		{
			FILE *file = fopen(args[1], "r");
			if (file == NULL)
			{
				perror(args[1]);
				result = 1;
			}
			else
			{
				char line[MAX_COMMAND_LENGTH];
				while (fgets(line, sizeof(line), file) != NULL)
				{
					line[strcspn(line, "\n")] = '\0';	// remove trailing newline
					if (strlen(line) > 0 && line[0] != '#')
					{
						int ret = run_command(line, output, output_size);
						if (ret != 0)
						{
							result = ret;
							break;
						}
					}
				}

				fclose(file);
			}
		}
	}
	else
	{
		// execute the command
		pid_t pid = fork();
		if (pid == 0)
		{
			// child process
			if (is_pipeline)
			{
				if (pipe(pipefd) == -1)
				{
					perror("pipe");
					exit(1);
				}

				pid_t pid2 = fork();
				if (pid2 == 0)
				{
				 		// grandchild process
					if (dup2(pipefd[1], STDOUT_FILENO) == -1)
					{
						perror("dup2");
						exit(1);
					}

					if (close(pipefd[0]) == -1)
					{
						perror("close");
						exit(1);
					}

					if (close(pipefd[1]) == -1)
					{
						perror("close");
						exit(1);
					}

					execvp(args[0], args);
					perror(args[0]);
					exit(1);
				}
				else if (pid2 == -1)
				{
					perror("fork");
					result = 1;
				}
				else
				{
				 		// child process
					if (dup2(pipefd[0], STDIN_FILENO) == -1)
					{
						perror("dup2");
						exit(1);
					}

					if (close(pipefd[0]) == -1)
					{
						perror("close");
						exit(1);
					}

					if (close(pipefd[1]) == -1)
					{
						perror("close");
						exit(1);
					}

					execvp(args2[0], args2);
					perror(args2[0]);
					exit(1);
				}
			}
			else
			{
				execvp(args[0], args);
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
			if (!is_background)
			{
				waitpid(pid, &status, 0);
				if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
				{
					result = 1;
				}
			}
			else
			{
				printf("Background process started with pid %d\n", pid);
			}
		}
	}

	return result;
}

int
run_commands(const char *commands, char *output, int output_size)
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
		int command_status =
			run_command(command_list[i], command_output, MAX_OUTPUT_LENGTH);

		if (command_status != 0)
		{
			result = command_status;
			break;
		}

		// append the output of the command to the result
		if (strlen(command_output) > 0)
		{
			if (output[0] != '\0')
			{
				strncat(output, " ", output_size - strlen(output) - 1);
			}

			strncat(output, command_output, output_size - strlen(output) - 1);
		}
	}

	// free memory allocated for the commands
	for (int i = 0; i < num_commands; i++)
	{
		free(command_list[i]);
	}

	return WIFEXITED(result) ? WEXITSTATUS(result) : -1;
}

int main(int argc, char **argv)
{
	printf("Welcome to mini-shell.\n");
	char command[MAX_COMMAND_LENGTH];
	char output[MAX_OUTPUT_LENGTH];
	char previous_command[MAX_COMMAND_LENGTH] = "";
	char previous_output[MAX_OUTPUT_LENGTH] = "";

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
			// print the previous command and its output
			if (strlen(previous_command) > 0)
			{
				printf("%s\n", previous_command);
				printf("%s\n", previous_output);

				// run the previous command and append its output to the result
				int result =
					run_commands(previous_command, output, MAX_OUTPUT_LENGTH);

				if (result != 0)
				{
					fprintf(stderr, "Error running previous command\n");
				}

				// save the previous command and its output as the previous previous command and output
				strncpy(previous_output, output, MAX_OUTPUT_LENGTH);
				output[0] = '\0';
			}
			else
			{
			 	// no previous command available
				fprintf(stderr, "No previous command available\n");
			}

			continue;
		}

		// run the command and get its return status
		int result = run_commands(command, output, MAX_OUTPUT_LENGTH);

		// print the output of the command
		int output_len = strlen(output);
		if (output_len > 0)
		{
			// remove any trailing newline characters from the command output
			if (output[output_len - 1] == '\n')
			{
				output[output_len - 1] = '\0';
			}

			printf("%s\n", output);
		}

		// save the command and its output as the previous command and output
		strncpy(previous_command, command, MAX_COMMAND_LENGTH);
		strncpy(previous_output, output, MAX_OUTPUT_LENGTH);

		// clear the output buffer
		output[0] = '\0';
	}

	return 0;
}
