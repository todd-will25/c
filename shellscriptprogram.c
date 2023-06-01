#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 512 // Maximum length of a command line
#define MAX_ARGS 10 // Maximum number of arguments to a command
#define MAX_COMMANDS 10 // # of commands in the file
#define MAX_HISTORY 20 //max of history memory storage

#define BUFFER_SIZE 1024 
#define ALIAS_USAGE \
    "Usage of alias:\n" \
    "alias                      - Display a list of all aliases\n" \
    "alias alias_name='command' - Add a new alias\n" \
    "alias -r alias_name        - Remove a single alias\n" \
    "alias -c                   - Remove all aliases" \

typedef struct Alias alias_t;
typedef struct Alias* alias_ptr_t;

// singly linked list to hold a list of alias names and commands
struct Alias
{
    char* name;
    char* command;
    alias_ptr_t next;
};

/* returns a pointer to a heap allocated alias struct
 filled with a copy of the passed name and command */
alias_ptr_t alias_create(char* name, char* command)
{
    alias_ptr_t new_alias_ptr = (alias_ptr_t) malloc(sizeof(alias_t));

    *new_alias_ptr =
        (alias_t) {
            .name = name,
            .command = command,
            .next = NULL
        };

    return new_alias_ptr;
}

// frees the particular alias struct
void alias_free(alias_ptr_t alias_ptr)
{
    if(alias_ptr != NULL)
    {
        free(alias_ptr->name);
        free(alias_ptr->command);

        *alias_ptr =
            (alias_t) {
                .name = NULL,
                .command = NULL,
                .next = NULL
            };

        free(alias_ptr);
    }
}

/* recursively frees the entire alias data structure with the
 passed argument as the head of the list */
alias_ptr_t alias_destroy(alias_ptr_t alias_ptr)
{
    if(alias_ptr)
    {
        alias_destroy(alias_ptr->next);
        alias_free(alias_ptr);
    }

    return NULL;
}

/* removes elements from the alias list that have a matching name
 and returns the new head of the list */
alias_ptr_t alias_remove(alias_ptr_t alias_ptr, const char* name)
{
    if(alias_ptr == NULL)
    {
        // empty list
        return NULL;
    }

    if(strcmp(alias_ptr->name, name) == 0)
    {
        // first element matches
        alias_ptr_t next = alias_ptr->next;
        alias_free(alias_ptr);
        return next;
    }

    // removing from the middle to the end of the list
    alias_ptr_t iterator = alias_ptr;

    while(iterator->next != NULL)
    {
        if(strcmp(iterator->next->name, name) == 0)
        {
            alias_ptr_t next = iterator->next->next;
            alias_free(iterator->next);
            iterator->next = next;
            break;
        }

        iterator = iterator->next;
    }

    return alias_ptr;
}

/* adds a new (name, command) pair to the list of aliases
 by first removing any element with the passed name and
 returns the new head of the list of aliases */
alias_ptr_t alias_add(alias_ptr_t alias_ptr, const char* name, const char* command)
{
    alias_ptr = alias_remove(alias_ptr, name);

    alias_ptr_t new_alias_ptr = alias_create(strdup(name), strdup(command));

    if(alias_ptr == NULL)
    {
        // list was empty
        return new_alias_ptr;
    }

    // going to the end of the alias list
    alias_ptr_t end_ptr = alias_ptr;
    while(end_ptr->next)
        end_ptr = end_ptr->next;

    // adding new_alias_ptr to the end
    end_ptr->next = new_alias_ptr;

    return alias_ptr;
}

// recursively displays the entire alias data structure
void alias_display(const alias_ptr_t alias_ptr)
{
    if(alias_ptr)
    {
        printf("%s=\"%s\"\n", alias_ptr->name, alias_ptr->command);
        alias_display(alias_ptr->next);
    }
}

/* recursively searches for a command with the passed name
 in the entire alias data structure and returns NULL if
 not found */
char* alias_query(const alias_ptr_t alias_ptr, const char* name)
{
    if(alias_ptr == NULL)
    {
        // not found
        return NULL;
    }

    if(strcmp(alias_ptr->name, name) == 0)
    {
        // match found
        return alias_ptr->command;
    }

    // search in the rest
    return alias_query(alias_ptr->next, name);
}

// executes non-alias-prefixed commands using system
void execute_other_command(char* command, const alias_ptr_t alias_ptr)
{
    char* query = alias_query(alias_ptr, command);

    if(query != NULL)
    {
        command = query;
    }

    system(command);
}

// executes alias commands
alias_ptr_t execute_alias_command(char* command, alias_ptr_t alias_ptr)
{
    bool incorrect_usage = false;

    if(strncmp(command, "alias -", strlen("alias -")) == 0)
    {
        // alias with options
        if(command[7] == 'c')
        {
            // remove all aliases
            alias_ptr = alias_destroy(alias_ptr);
        }
        else if(command[7] == 'r')
        {
            // remove a single alias
            char alias_name[BUFFER_SIZE];
            sscanf(command, "alias -r %s", alias_name);
            alias_ptr = alias_remove(alias_ptr, alias_name);
        }
        else
        {
            incorrect_usage = true;
        }
    }
    else if(command[5] != '\0')
    {
        // set alias
        char* alias_name;
        char* alias_command;

        // start of alias name
        alias_name = command + 6;

        // finding assignment operator
        char* iterator = alias_name + 1;
        bool assignment_found = false;
        while(*iterator != '\0')
        {
            if(*iterator == '=')
            {
                assignment_found = true;
                break;
            }

            ++iterator;
        }

        if(assignment_found)
        {
            *iterator = '\0'; // replacing assignment operator with '\0'
            ++iterator;

            // quote at start of alias command
            if(*iterator == '\'')
            {
                alias_command = ++iterator;

                // finding ending quote after alias command
                bool quote_found = false;
                while(*iterator != '\0')
                {
                    if(*iterator == '\'')
                    {
                        quote_found = true;
                        break;
                    }

                    ++iterator;
                }

                if(quote_found)
                {
                    *iterator = '\0'; // replacing quote with '\0'

                    // adding alias
                    alias_ptr = alias_add(alias_ptr, alias_name, alias_command);
                }
                else
                {
                    incorrect_usage = true;
                }
            }
            else
            {
                incorrect_usage = true;
            }
        }
        else
        {
            incorrect_usage = true;
        }
    }
    else
    {
        // display aliases
        alias_display(alias_ptr);
    }

    if(incorrect_usage)
    {
        // incorrect usage
        puts("Incorrect usage.");
        puts(ALIAS_USAGE);
    }

    return alias_ptr;
}


void execute_commands(char** args, int input_fd, int output_fd) {

	pid_t pid = fork(); // Create a child process

    if (pid == 0) { // Child process
        // Redirect input/output streams if necessary
        if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }
        // Execute the command
        execvp(args[0], args);
        perror("execvp"); // Print an error message if execvp fails
        exit(1);
    } else if (pid > 0) { // Parent process
        int status;
        waitpid(pid, &status, 0); // Wait for the child process to finish
    } else { // Error forking
        perror("fork");
        exit(1);
    }
	
}

int main(int argc, char* argv[]) {

    char line[MAX_LINE]; // Buffer to hold command line input
    char* args[MAX_ARGS]; // Array to hold command and arguments
    char *history[MAX_HISTORY]; //Array for history commands
	int history_count = 0; //counter to keep track of how many commands have been inputed
	int fd_in, fd_out; //file redirection
	alias_ptr_t alias_ptr = NULL;
	char aliascommand[BUFFER_SIZE];
	char *command[MAX_COMMANDS][MAX_ARGS]; // array to hold commands for batch mode
	int num_commands = 0; //# of commands in batch mode
	int batch_mode = 0; //batch mode indicator
	FILE* batch_file = NULL; //pointer for batch mode
	
	//path creation
    char* path = getenv("PATH");
    char path_copy[strlen(path) + 1];
    strcpy(path_copy, path);

	if(argc == 2) {
		batch_mode = 1;
		batch_file = fopen(argv[1], "r");
		if (batch_file == NULL) {
			printf("ERROR: could not open batch file\n");
			exit(1);
		}
	}
	if (batch_mode) {
		while (fgets(line, MAX_LINE, batch_file) != NULL) {
        	// Parse command and arguments
        	int num_args = 0;
        	char* token = strtok(line, " \n");
        	while (token != NULL) {
            	args[num_args] = token;
            	num_args++;
            	token = strtok(NULL, " \n");
        }
			args[num_args] = NULL;
			// Execute command
			execute_commands(args, STDIN_FILENO, STDOUT_FILENO);
        }
			// Close batch file
			fclose(batch_file);
	}
	else{

	    while (1) {
            // Display prompt and read command line input
            if(!batch_mode){
                printf("prompt> ");
        	    fgets(line, MAX_LINE, stdin);
            
            //history operation
			    if(line[0] != '\n') {
				    if(history_count < MAX_HISTORY) {
					    history[history_count] = strdup(line);
					    history_count++;
				    }
				    else{
					    free(history[0]);
					    for(int i=1; i < MAX_HISTORY; i++){
						    history[i-1] = history[i];
					    }
					    history[MAX_HISTORY-1] = strdup(line);
				    }
			    }

                // Parse command line input into individual arguments
                char* token = strtok(line, " \n");
                int i = 0;
                while (token != NULL && i < MAX_ARGS-1) {
                    args[i] = token;
                    token = strtok(NULL, " \n");
                    i++;
                }

                args[i] = NULL; // Set last argument to NULL for execvp

                //built in commands
                //cd
                if (strcmp(args[0], "cd") == 0) {
            // Change working directory
            if (args[1] == NULL) {
                chdir(getenv("HOME"));
            } else {
                chdir(args[1]);
            }
            } else if (strcmp(args[0], "exit") == 0) //if true then exit
            {
                if (args[1] == NULL) 
                {
                    exit(0);
                 } 
                else {
                    int status;
                    pid_t pid = fork();
                if (pid == 0) //executing command in child
                {
                    // If execvp returns, an error occurred
                    execvp(args[1], &args[1]);
                    printf("Error: command not found\n");
                    exit(1);
                } else if (pid > 0) { //waiting for child process
                    wait(&status);
                    exit(0);
                } else //if fork failed 
                {
                    printf("Error: could not fork process\n");
                }
            }
        } 
  		else if (strcmp(args[0], "path") == 0) {
              // Show current path
              if (args[1] == NULL) {
                  printf("%s\n", path);
              } else {
                  // Append or remove pathname
                  if (strcmp(args[1], "+") == 0 && args[2] != NULL) {
                      strcat(path_copy, ":");
                      strcat(path_copy, args[2]);
                      path = path_copy;
                      setenv("PATH", path, 1);
                  } else if (strcmp(args[1], "-") == 0 && args[2] != NULL) {
                      char* path_ptr = path_copy;
                      char* path_elem = strtok(path_copy, ":");
                      int found = 0;
                      while (path_elem != NULL) {
                          if (strcmp(path_elem, args[2]) != 0) {
                              strcat(path_ptr, path_elem);
                              strcat(path_ptr, ":");
                          } else {
                              found = 1;
                          }
                          path_elem = strtok(NULL, ":");
                      }
                      if (found) {
                          path_ptr[strlen(path_ptr) - 1] = '\0'; // Remove trailing colon
                          path = path_ptr;
                          setenv("PATH", path, 1);
                      } else {
                          printf("Error: path element not found\n");
                      }
                  }
  			}
  		}
                //myHistory built in command
		        else if (strcmp(args[0], "myhistory") == 0) 
                {
                    // Show command history
                    if (args[1] == NULL) 
                    {
                        for (int i = 0; i < history_count; i++) //max amount of history storage is 20 
                        {
                            printf("%d %s", i+1, history[i]);
                        }
                    } 
                    else if (strcmp(args[1], "-c") == 0) 
                    {
                        // Clear command history
                        for (int i = 0; i < history_count; i++) 
                        {
                            free(history[i]);
                        }
                        history_count = 0;
                    } 
                    else if (strcmp(args[1], "-e") == 0 && args[2] != NULL) 
                    {
				        // Execute command from certain place in queue
				        int index = atoi(args[2]); // Get index of command to execute
				        if (index < 1 || index > history_count) {
                            //print error message for invalid entries 
						    printf("Error: invalid history index\n");
				        } 
                        else 
                        {
				        // Copy command to args array and execute
					        char* command = history[(history_count + index - 1) % MAX_HISTORY];
					        int arg_count = 0;
					        char* token = strtok(command, " \n");
				            while (token != NULL && arg_count < MAX_ARGS-1) 
                            {
					            args[arg_count] = token;
					            token = strtok(NULL, " \n");
					            arg_count++;
				            }
				        args[arg_count] = NULL; // Set last argument to NULL for execvp
				        }
			        }
		        }
                else if (strcmp(args[0], "who") == 0) 
                {
                    int fd_in = -1, fd_out = -1;
                    if (strcmp(args[1], "<") == 0)
                    {
                    fd_in = open(args[2], O_RDONLY);
                        if (fd_in == -1) 
                        {
                        perror("open");
                        exit(EXIT_FAILURE);
                        }
                        args[i] = NULL;
                    } 
                    else if (strcmp(args[1], ">") == 0) 
                    {
                        fd_out = open(args[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if (fd_out == -1) 
                        {
                        perror("open");
                        exit(EXIT_FAILURE);
                        }
                        args[i] = NULL;
                    }
                // execute command with input/output redirection
                    pid_t pid = fork();
                    if (pid == -1) {
                        perror("fork");
                        exit(EXIT_FAILURE);
                    } else if (pid == 0) {  // child process
                    if (fd_in != -1) {
                        if (dup2(fd_in, STDIN_FILENO) == -1) {
                            perror("dup2");
                            exit(EXIT_FAILURE);
                        }
                        close(fd_in);
                    }
                    if (fd_out != -1) {
                        if (dup2(fd_out, STDOUT_FILENO) == -1) {
                            perror("dup2");
                            exit(EXIT_FAILURE);
                        }
                        close(fd_out);
                    }
                    execvp(args[0], args);
                    perror("execvp");
                    exit(EXIT_FAILURE);
                    } else {  // parent process
                    wait(NULL);
                    }
                }
             else if (strcmp(args[0], "who") == 0 && args[1] != NULL && strcmp(args[1], "|") == 0 && strcmp(args[2],  "grep") == 0 && args[3] != NULL && strcmp(args[4], ">") == 0 && args[5] != NULL) {
            // Piping and redirection
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                perror("pipe");
                exit(1);
            }

            pid_t child1 = fork();
            if (child1 == -1) {
                perror("fork");
                exit(1);
            } else if (child1 == 0) {
                // Child process 1 - execute "who" command and write output to pipe
                close(pipefd[0]); // Close unused read end of pipe
                dup2(pipefd[1], STDOUT_FILENO); // Redirect output to write end of pipe
                close(pipefd[1]); // Close write end of pipe

                execvp(args[0], args);
                perror("execvp");
                exit(1);
            } else {
                pid_t child2 = fork();
                if (child2 == -1) {
                perror("fork");
                exit(1);
            } else if (child2 == 0) {
                // Child process 2 - execute "grep" command and read input from pipe
                close(pipefd[1]); // Close unused write end of pipe
                int out_fd = open(args[5], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (out_fd == -1) {
                    perror("open");
                    exit(1);
                }
                dup2(out_fd, STDOUT_FILENO); // Redirect output to output.txt file
                close(out_fd); // Close file descriptor for output.txt
                dup2(pipefd[0], STDIN_FILENO); // Redirect input to read end of pipe
                close(pipefd[0]); // Close read end of pipe

                execvp(args[2], args+2);
                perror("execvp");
                exit(1);
            	}
            	else {
                	// Parent process - close pipe ends and wait for child processes to complete
                	close(pipefd[0]); // Close unused read end of pipe
                	close(pipefd[1]); // Close unused write end of pipe

                	waitpid(child1, NULL, 0);
                	waitpid(child2, NULL, 0);
            	}
            }
            }
            else if(strncmp(line, "alias", strlen("alias")) == 0)
            {
                line[strlen(line) - 1] = '\0'; //removing newline character
                char *arg1 = strtok(line + strlen("alias "), "="); // extract alias name
                    char *arg2 = strtok(NULL, ""); // extract command
                    if (arg1 == NULL || arg2 == NULL) { // incorrect usage
                        printf("%s\n", ALIAS_USAGE);
                    } else if (strcmp(arg2, "") == 0) { // remove alias
                        alias_ptr = alias_remove(alias_ptr, arg1);
                    } else { // add alias
                        alias_ptr = alias_add(alias_ptr, arg1, arg2);
                    }
                   }else if (strcmp(line, "alias") == 0) { // display list of all aliases
                    alias_display(alias_ptr);
                   } else if (strcmp(line, "exit") == 0) { // exit shell
                    break;
                } else {
                    // execute non-alias-prefixed commands using system
                    execute_other_command(line, alias_ptr);
                    }
             }
		    else {
                    // Execute external command
                    int status;
                    pid_t pid = fork();
                    if (pid == 0) {
                        // Execute command in child process
                        execvp(args[0], args);
                        // If execvp returns, an error occurred
                        printf("Error: command not found\n");
                        exit(1);
                    } 
                    else if (pid > 0) {
                        // Wait for child process to finish
                        wait(&status);
                    } 
                    else {
                        // Fork failed
                        printf("Error: could not fork process\n");
			        }
			    }
            }


        }


    // destroying the alias
    alias_ptr = alias_destroy(alias_ptr);
    return 0;
}

