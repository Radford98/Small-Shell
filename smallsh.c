/* Author: Brad Powell
 * Date: 5/16/2019
 * CS344_400 Spring 2019
 * smallsh is a small shell. It has a few built in commands, uses fork/exec to run other programs, and
 * handles some signals.
 */

#define _POSIX_C_SOURCE 200809L		// Header declaration for getline
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

void main() {
	// Variables for getting user input
	int numEnt = -5;	// Number of characters entered
	size_t buffer = 0;	// Size of allocated buffer
	char* command = NULL;	// Entered string + \n + \0 

	char tokenize[2049];	// Copies the command for tokenization, maximum of 2048 chars + \n
	char* argArr[512];	// Array for arguments, maximum number of arguments is 512
	char delim[] = " ";	// Deliminater for strtok
	int numArgs;		// Holds the number of arguments

//while(1) {	// Keep asking for commands until exit
	// Display command prompt and wait for the user
	printf(": ");
	fflush(stdout);
	
	numEnt = getline(&command, &buffer, stdin);
	// Remove newline from command
	command[strcspn(command, "\n")] = '\0';
	strcpy(tokenize, command);
	
	// Grab first argument
	argArr[0] = strtok(tokenize, delim);
	// Check for built in commands
	if (command[0] == '#') {//(strcmp(command[0], "#") == 0) {
		// Ignore the line
	} else if (strcmp(argArr[0], "exit") == 0) {
		// Implement exit (kill processes)
		return;
	} else if (strcmp(argArr[0], "cd") == 0) {
		// Implement cd
	} else if (strcmp(argArr[0], "status") == 0) {
		// Implement status
	} else { // Run whatever command came through
		// Parse the given command
		numArgs = 1;	// Index for array
		argArr[numArgs] = strtok(NULL, delim); // Pull next argument
		// Loop over argument list until there are no arguments or redirection/background is detected
		while (argArr[numArgs] != NULL
				&& strcmp(argArr[numArgs], "<") != 0
				&& strcmp(argArr[numArgs], ">") != 0
				&& strcmp(argArr[numArgs], "&") != 0) {
			numArgs++;			// Increment array index
			argArr[numArgs] = strtok(NULL, delim);
		}

		for (int i = 0; i < numArgs; i++) {
			printf("Argument %d: %s\n", i, argArr[i]);
		}
		printf("Total arguments: %d\n", numArgs);

	}
	/* Playing with strtok
	char delim[] = " ";
	char* array[3];
	array[0] = strtok(command, delim);
	array[1] = strtok(NULL, delim);
	array[2] = strtok(NULL, delim);
	printf("Tokens: '%s', '%s', '%s'\n", array[0], array[1], array[2]);

	int size = strcspn(orig, "a");
	printf("First 'a' of original string is at index %d\n", size);
	printf("Should be 'a': %c\n", orig[size]);
	*/

	free(command);
	command = NULL;
//}
}
