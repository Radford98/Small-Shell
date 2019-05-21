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


int ParseArgs(char**, char*);		// Accepts array for arguments and string to parse; Returns number of args
void RedirectFile(char*, char[], int);	// Parses the file name for redirection.
void ExpandPID(char**, char*, int*);	// Function to expand $$ to PID

void main() {
	// Variables for getting user input
	int numEnt = -5;	// Number of characters entered
	size_t buffer = 0;	// Size of allocated buffer
	char* command = NULL;	// Entered string + \n + \0 
	int freed;		// Tracks if command has been freed (in the case of expanding $$)

	// Variable for expanding $$,
	char newCmd[2058];	// New command, max size of input plus space for PID

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
	int bg = 0;		// 'bool' for running background processes
	pid_t pidArr[20];	// Array to hold background PIDs
	int numBG = 0;		// Int to keep track of number of bg processes

	// Variables for file redirection
	char fileName[100];

while(1) {	// Keep asking for commands until exit
	freed = 0;	// Command starts unfreed

	// Check for background children for reaping.
	for (int i = 0; i < numBG; i++){
		pid_t reap = waitpid(pidArr[i], &childExitMethod, WNOHANG);

		if (reap != 0) {
			// Print beginning of background message
			printf("background pid %d is done: ", reap);
			fflush(stdout);

			// Retrieve exitStatus/termSignal and print results.
			if(WIFEXITED(childExitMethod)) {
				exitStatus = WEXITSTATUS(childExitMethod);
				termSignal = 0;
				printf("exit value %d\n", exitStatus);
				fflush(stdout);
			} else {
				termSignal = WTERMSIG(childExitMethod);
				printf("terminated by signal %d\n", termSignal);
				fflush(stdout);
			}
		
			// We no longer need to hold on to that pid. The last pid is copied into this pid's location,
			// then numBG is decremented (so the same pid isn't checked twice).
			// i is also decremented so it checks the current index again
			// (because the new pid here might also need to be reaped).
			pidArr[i] = pidArr[numBG-1];
			numBG--;
			i--;
		}
	}

	// Display command prompt and wait for the user
	printf(": ");
	fflush(stdout);
	
	numEnt = getline(&command, &buffer, stdin);
	// Ignore empty or commented lines
	if (numEnt == 1 || command[0] == '#')
		continue;
	// Remove newline from command
	command[strcspn(command, "\n")] = '\0';

	// Expand $$ to PID
	if (strstr(command, "$$") != NULL) {
		ExpandPID(&command, newCmd, &numEnt);
		freed = 1;	// if the above function runs even once, command was already freed.
	}

	// Parse commandline
	numArgs = ParseArgs(argArr, command);

	// Check for built in commands
	if (strcmp(argArr[0], "exit") == 0) {
		// kill all children
		for (int i = 0; i < numBG; i++) {
			kill(pidArr[i], SIGKILL);
		}
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

		/* First check if the process is in the foreground or background
		 If the final char is not & or there isn't a space before it (making
		 it a separate word), it runs in the foreground.	*/
		if (command[numEnt-2] != '&' || command[numEnt-3] != ' ') {
			bg = 0;
		} else {
			bg = 1;
		}				
	

		spawnid = fork();
		switch (spawnid) {
			case -1:
				perror("Fork failed");
				exit(1);
				break;
			case 0:	// Child process
				/* Check for redirection. strrchr locates the last occurence of a redirection
				character (incase they're used incidientally in argument names) and,
				after some pointer arithmatic,  verifies that it's a separate word by
				checking the chars before and after it. Then, pull the actual destination/source
				from the command string.
				If a destination is not specified and it runs in the background, redirect
				to dev/null.	*/
				
				// Check for input redirection
				if (strrchr(command, '<') != NULL) {
					redInd = strrchr(command, '<') - command;
					if (command[redInd-1] == ' ' && command[redInd+1] == ' ') {
						RedirectFile(command, fileName, redInd);

						int srcFD = open(fileName, O_RDONLY);
						if (srcFD == -1) {
							printf("File not found\n");
							fflush(stdout);
							exit(1);
						}
						dup2(srcFD, 0);	// Redirect stdin to the specified file.
					}
				} else if (bg == 1) {
					// If process is run in background with no other redirection,
					// redirect to dev/null
					int devNull = open("/dev/null", O_RDONLY);
					dup2(devNull, 0);
				}

				// Check for output redirection
				if (strrchr(command, '>') != NULL) {
					redInd = strrchr(command, '>') - command;
					if (command[redInd-1] == ' ' && command[redInd+1] == ' ') {
						RedirectFile(command, fileName, redInd);

						int destFD = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
						if (destFD == -1) {
							printf("Error writing file\n");
							fflush(stdout);
							exit(1);
						}
						dup2(destFD, 1);
					}
				} else if (bg == 1) {
					int devNull = open("/dev/null", O_WRONLY);
					dup2(devNull, 1);
				}


				// Run exec
				execvp(argArr[0], argArr);
				printf("Error, command not found.\n");
				fflush(stdout);
				exit(1);
				break;
			default:
				// First check if the process is in the foreground or background.
				// Wait if it runs in the foreground.
				// Print the process id if it's in the background and add to bgpid array
				if (bg == 0) {
					waitpid(spawnid, &childExitMethod, 0);
					if(WIFEXITED(childExitMethod)){
						exitStatus = WEXITSTATUS(childExitMethod);
						termSignal = 0;
					} else {
						termSignal = WTERMSIG(childExitMethod);
					}
				} else {	// Child is in the background, keep going.
					// Add child PID to array of child PIDs
					pidArr[numBG] = spawnid;
					numBG++;

					printf("background pid is %d\n", spawnid);
					fflush(stdout);

				}
				break;
		}
		
	}
	if (freed == 0) {
		free(command);
	}
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
void RedirectFile(char* command, char fileName[], int i) {
	memset(fileName, '\0', 100);	// Reset filename
	i += 2;	// Sets the index to the first character of the file name (skipping over the space)
	// This loop builds up the string.
	while (command[i] != ' ' && command[i] != '\0') {
		fileName[strlen(fileName)] = command[i];
		i++;
	}
}

/* This function accepts the original command line, an array to hold the expanded command, and the address
 * of the int to track the length of the command. After the function runs, command has been freed and now points
 * to the same address as newCmd, so 'command' can be used in the rest of the program regardless if this
 * function needs to run or not. The int tracking the length of the command is updated to the new length.
 * NOTE: Since the value of command is being changed (not just the value it points to), this function needs
 * its address and works through dereferencing.
 */
void ExpandPID(char** command, char* newCmd, int* numEnt){
	char pid[10];			// Stringified PID
	sprintf(pid, "%d", getpid());	// Convert pid into a string
	int pidLoc;			// Index of $$ in command
	char* pidPtr;			// Pointer to $$ in command
	int f = 0;			// Internal 'freed' variable so command isn't freed multiple times.

	pidPtr = strstr(*command, "$$");
	while (pidPtr != NULL) {
		strcpy(newCmd, *command);	// Copy the command string into a larger string for editing
		pidLoc = pidPtr - *command;	// Index for $$
		// memmove copies bytes - in this case to move bytes further down the string.
		// It moves everything after the $$ down to make room for the PID.
		memmove(&newCmd[pidLoc+strlen(pid)], &newCmd[pidLoc+2], *numEnt-pidLoc-2);

		strncpy(&newCmd[pidLoc], pid, strlen(pid));	// Insert PID into newCmd

		if (f == 0) {	// Free allocated command if it hasn't been already
			free(*command);
			f = 1;
		}

		*command = newCmd;
		// The number of characters has increase by PID, but $$ was removed

		*numEnt = *numEnt+strlen(pid)-2;
		pidPtr = strstr(*command, "$$");
	}
}
