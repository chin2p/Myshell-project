#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    char *path = getenv("PATH");   // get the PATH environment variable
    char *dir;                     // pointer to each directory in the PATH
    char *cmd;                     // pointer to the command we're searching for
    char prog[256];                // buffer to hold the full path to the command
    int found = 0;                 // flag to indicate if we've found the command
    
    // check if the PATH environment variable exists
    if (path == NULL) {
        fprintf(stderr, "Error: PATH environment variable is not set.\n");
        exit(1);
    }
    
    // copy the command we're searching for (i.e. "ls") into a buffer
    cmd = argv[1];
    
    // loop through each directory in the PATH
    while ((dir = strsep(&path, ":")) != NULL) {
        // build the full path to the command by concatenating the directory and command names
        sprintf(prog, "%s/%s", dir, cmd);
        
        // check if the file exists and is executable
        if (access(prog, X_OK) == 0) {
            // found the command, set the flag and break out of the loop
            found = 1;
            break;
        }
    }
    
    if (!found) {
        fprintf(stderr, "Error: %s command not found.\n", cmd);
        exit(1);
    }
    
    // execute the command with any arguments that were passed in
    execv(prog, argv + 1);
    
    // if execv returns, there was an error
    fprintf(stderr, "Error: failed to execute %s.\n", cmd);
    exit(1);
}
