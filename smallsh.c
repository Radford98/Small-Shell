/* Author: Brad Powell
 * Date: 5/21/2019
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

// Global variables for SIGTSTP handling
int waiting = 0, sigRec = 0, fgOnly = 0;

int ParseArgs(char**, char*);		// Accepts array for arguments and string to parse; Returns number of args
void RedirectFile(char*, char[], int);	// Parses the file name for redirection.
void ExpandPID(char**, char*, int*);	// Function to expand $$ to PID
int ReapChildren(pid_t[], int);		// Waits for background processes
void catchSIGTSTP(int);			// Handles SIGTSTP

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

	// Variable for status command
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

	// Set up signal handling
	struct sigaction default_action = {0}, ignore_action = {0}, SIGTSTP_action = {0};

	default_action.sa_handler = SIG_DFL;
	ignore_action.sa_handler = SIG_IGN;

	SIGTSTP_action.sa_handler = catchSIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = 0;

	// Parent ignores SIGINT and handles terminated foreground child with childExitMethod
	sigaction(SIGINT, &ignore_action, NULL);
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

while(1) {	// Keep asking for commands until exit
	freed = 0;	// Command starts unfreed

	// Check for background children for reaping.
	numBG = ReapChildren(pidArr, numBG); 

	// Get input, guarding against signals.
	while(1) {
		// Display command prompt and wait for the user
		printf(": ");
		fflush(stdout);

		numEnt = getline(&command, &buffer, stdin);
		if (numEnt == -1) {
			clearerr(stdin);
		} else {
			break;
		}
	}

	
	// Ignore empty or commented lines, returning to top of loop
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
		The final character must be an & and the char in front of it must be a space
		for it to be a background command. Also, the shell must not be running ing
		foreground-only mode.	*/

		if (command[numEnt-2] == '&' && command[numEnt-3] == ' ' && fgOnly == 0) {
			bg = 1;
		} else {
			bg = 0;
		}

		spawnid = fork();
		switch (spawnid) {
			case -1:
				perror("Fork failed");
				exit(1);
				break;
			case 0: ;	// Child process

				// Set child processes to ignore SIGTSTP
				sigaction(SIGTSTP, &ignore_action, NULL);

				// Foreground children do not ignore SIGINT
				// Background children ignoring SIGINT set up at beginning of program
				if (bg == 0) {
					sigaction(SIGINT, &default_action, NULL);
				}

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
			default:// Parent process
				// First check if the process is in the foreground or background.
				// Wait if it runs in the foreground.
				// Print the process id if it's in the background and add to bgpid array
				if (bg == 0) {
					// If TSTP comes during a wait, resuming waiting
					SIGTSTP_action.sa_flags = SA_RESTART; 
					sigaction(SIGTSTP, &SIGTSTP_action, NULL);

					waiting = 1;	// Set to ignore SIGTSTP until fg child is finished
					waitpid(spawnid, &childExitMethod, 0);
					if(WIFEXITED(childExitMethod)){
						exitStatus = WEXITSTATUS(childExitMethod);
						termSignal = 0;
					} else {
						termSignal = WTERMSIG(childExitMethod);
						printf("terminated by signal %d\n", termSignal);
						fflush(stdout);
					}

					// Reset signal handler for getline
					SIGTSTP_action.sa_flags = 0;
					sigaction(SIGTSTP, &SIGTSTP_action, NULL);


					waiting = 0;	// Finished waiting
					if (sigRec == 1) {
						sigRec = 0;	// Reset now that the signal is received
						raise(SIGTSTP);	// Resend TSTP
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

/* ReapChildren accepts an array of background pids and an int tracking the number of pids in that array.
 * It loops through the array, waiting (no hang) on each one in turn and lets the user know if any
 * children have been reaped.
 * Also manages its array to get rid of values it no longer needs to track.
 * Returns the new number of pids in the array.
 */
int ReapChildren(pid_t pidArr[], int numBG) {
	pid_t reap;
	int childExitMethod;
	int deathNum;

	for (int i = 0; i < numBG; i++){
		reap = waitpid(pidArr[i], &childExitMethod, WNOHANG);

		if (reap != 0) {
			// Print beginning of background message
			printf("background pid %d is done: ", reap);
			fflush(stdout);

			// Retrieve exitStatus/termSignal and print results.
			if(WIFEXITED(childExitMethod)) {
				deathNum = WEXITSTATUS(childExitMethod);
				printf("exit value %d\n", deathNum);
				fflush(stdout);
			} else {
				deathNum = WTERMSIG(childExitMethod);
				printf("terminated by signal %d\n", deathNum);
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

	return numBG;	// Return the new number of background pids in array.
}

/* Function to catch Ctrl-Z (TSTP). It first checks if the parent is waiting on a foreground process and, if so
 * leaves a message so the parent knows to call the function back. When the parent is free, it displays a
 * message about entering or exiting foreground-only mode.
 */
void catchSIGTSTP(int signo) {
	// Handle foreground waiting
	if (waiting == 1) {
		sigRec = 1;	// Let the main know TSTP was sent
	} else {
		// Change between normal and foreground only
		if (fgOnly == 0) {
			fgOnly = 1;
			char* message = "Entering foreground-only mode (& is now ignored)\n";
			write(STDOUT_FILENO, message, 49);
		} else {
			fgOnly = 0;
			char* message = "Exiting foreground-only mode\n";
			write(STDOUT_FILENO, message, 29);
		}
		fflush(stdout);
	}
}

