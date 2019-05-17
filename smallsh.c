/* Author: Brad Powell
 * Date: 5/16/2019
 * CS344_400 Spring 2019
 * smallsh is a small shell. It has a few built in commands, uses fork/exec to run other programs, and
 * handles some signals.
 */

#define _POSIX_C_SOURCE 200809L		// Header declaration for getline... apparently.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>


int ParseArgs(char**, char*);	// Accepts array for arguments and string to parse; Returns number of arguments
void RedirectFile(char*, char*, int);	// Parses the file name for redirection.

void main() {
	// Variables for getting user input
	int numEnt = -5;	// Number of characters entered
	size_t buffer = 0;	// Size of allocated buffer
	char* command = NULL;	// Entered string + \n + \0 

	// Variables storing arguments
	char* argArr[512];	// Array for arguments, maximum number of arguments is 512
	int numArgs;		// Holds the number of arguments

	// Variables for status command
	int	exitStatus = 0,
		termSignal = 0;

	// Variables for fork
	pid_t spawnid = -5;
	int childExitMethod = -5;
	int redInd = 0;

	// Variables for file redirection
	char fileName[100];

while(1) {	// Keep asking for commands until exit
	// Display command prompt and wait for the user
	printf(": ");
	fflush(stdout);
	
	numEnt = getline(&command, &buffer, stdin);
	// Ignore empty or commented lines
	if (numEnt == 1 || command[0] == '#')
		continue;
	// Remove newline from command
	command[strcspn(command, "\n")] = '\0';

	// Parse commandline
	numArgs = ParseArgs(argArr, command);

	// Check for built in commands
	if (strcmp(argArr[0], "exit") == 0) {
		// Implement exit (kill processes)
		return;
	} else if (strcmp(argArr[0], "cd") == 0) {
		int dirErr;
		// If cd is used by itself, change to the HOME directory
		if (numArgs == 1) {
			dirErr = chdir(getenv("HOME"));
		} else {	// Otherwise, change to the directory passed to it.
			dirErr = chdir(argArr[1]);	// cd only accepts one argument, so cd to that
		}
		
		if (dirErr == -1) {
			printf("Could not find the directory.\n");
			fflush(stdout);
		} 
	} else if (strcmp(argArr[0], "status") == 0) {		// Print the status of last foreground process
		if (termSignal == 0) {
			printf("exit value %d\n", exitStatus);
			fflush(stdout);
		} else {
			printf("terminated by signal %d\n", termSignal);
			fflush(stdout);
		}
	} else {
		// Run whatever command came through
		spawnid = fork();
		switch (spawnid) {
			case -1:
				perror("Fork failed");
				exit(1);
				break;
			case 0:	// Child process

				/*
 				Check for redirection. strrchr locates the last occurence of a redirection
				character (incase they're used incidientally in argument names) and,
				after some pointer arithmatic,  verifies that it's a separate word by
				checking the chars before and after it. Then, pull the actual destination/source
				from the command string.
				Commences redirection.
				*/

				// Check for input
				if (strrchr(command, '<') != NULL) {
					redInd = strrchr(command, '<') - command;
					if (command[redInd-1] == ' ' && command[redInd+1] == ' ') {
						RedirectFile(command, fileName, redInd);

						int srcFD = open(fileName, O_RDONLY);
						if (srcFD == -1) {
							printf("File not found\n");
							exit(1);
						}
						dup2(srcFD, 0);	// Redirect stdin to the specified file.
					}
				}

				// Check for output
				if (strrchr(command, '>') != NULL) {
					redInd = strrchr(command, '>') - command;
					if (command[redInd-1] == ' ' && command[redInd+1] == ' ') {
						RedirectFile(command, fileName, redInd);

						int destFD = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
						if (destFD == -1) {
							printf("Error writing file\n");
							exit(1);
						}
						dup2(destFD, 1);
					}
				}


				// Run exec
				execvp(argArr[0], argArr);
				printf("Error, command not found.\n");
				exit(1);
				break;
			default:
				// First check if the process is in the foreground or background
				// If the final char is not & or there isn't a space before it (making
				// it a separate word), it runs in the foreground and the shell
				// must wait.
				if (command[numEnt-2] != '&' || command[numEnt-3] != ' ') {
					waitpid(spawnid, &childExitMethod, 0);
					if(WIFEXITED(childExitMethod)){
						exitStatus = WEXITSTATUS(childExitMethod);
						termSignal = 0;
					} else {
						termSignal = WTERMSIG(childExitMethod);
					}
				} else {
					// run in background
				}
				break;
		}
		
	}

	free(command);
	command = NULL;
	
}
	return;
}


int ParseArgs(char** array, char* parseMe) {
	int args = 0;		// Array index and number of arguments
	char tokenize[2048];	// Copies the command for tokenization, maximum of 2048 chars
	char delim[] = " ";	// Deliminater
	
	strcpy(tokenize, parseMe);
	array[args] = strtok(tokenize, delim);

	// Loop over argument list until there are no arguments left or redirection/background is detected
	while (array[args] != NULL
			&& strcmp(array[args], "<") != 0
			&& strcmp(array[args], ">") != 0
			&& strcmp(array[args], "&") != 0) {
		args++;			// Increment array index
		array[args] = strtok(NULL, delim);
	}

	array[args] = NULL;	// Set the last element in the array to NULL for exec...()

	return args;
}

// This function accepts the command line and the index of the redirection symbol and returns
// the file name that the shell needs to redirect to.						
void RedirectFile(char* command, char*fileName, int i) {
	memset(fileName, '\0', sizeof(fileName));	// Reset filename
	i += 2;	// Sets the index to the first character of the file name (skipping over the space)
	// This loop builds up the string.
	while (command[i] != ' ' && command[i] != '\0') {
		fileName[strlen(fileName)] = command[i];
		i++;
	}
}
