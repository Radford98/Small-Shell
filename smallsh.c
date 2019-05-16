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

int ParseArgs(char**, char*);	// Accepts array for arguments and string to parse; Returns number of arguments

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
		} 
	} else if (strcmp(argArr[0], "status") == 0) {		// Print the status of last foreground process
		if (termSignal == 0) {
			printf("exit value %d\n", exitStatus);
		} else {
			printf("terminated by signal %d\n", termSignal);
		}
	} else { // Run whatever command came through
		for (int i = 0; i < numArgs; i++) {
			printf("Argument %d: %s\n", i, argArr[i]);
		}
		printf("Total arguments: %d\n", numArgs);
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

	return args;
}
