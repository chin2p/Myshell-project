#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>


void execute_command(char** args) {
    // commands here
    printf("Command executed: %s\n", args[0]);
}

void batch_mode(FILE* fp) {
    char command[1024];
    while (fgets(command, 1024, fp) != NULL) {
        // Remove newline character from the end of the command
        command[strcspn(command, "\n")] = '\0';

        // Split the command into arguments
        char* args[2];
        char* token = strtok(command, " ");
        int num_args = 0;
        while (token != NULL && num_args < 2) {
            args[num_args++] = token;
            token = strtok(NULL, " ");
        }
        args[num_args] = NULL;

        // Execute the command
        execute_command(args);

        // Exit if the command is "exit"
        if (strcmp(args[0], "exit") == 0) {
            break;
        }
    }
}

void interactive_mode() {
    printf("Welcome to mysh!\n");
    while (1) {
        printf("mysh> ");

        // Read input from user
        char command[1024];
        fgets(command, 1024, stdin);

        // Remove newline character from the end of the command
        command[strcspn(command, "\n")] = '\0';

        // Exit if the command is "exit"
        if (strcmp(command, "exit") == 0) {
            break;
        }

        // Split the command into arguments
        char* args[2];
        char* token = strtok(command, " ");
        int num_args = 0;
        while (token != NULL && num_args < 2) {
            args[num_args++] = token;
            token = strtok(NULL, " ");
        }
        args[num_args] = NULL;

        // Execute the command
        execute_command(args);
    }
    printf("Goodbye!\n");
}

int main(int argc, char** argv) {
    if (argc > 1) {
        // Batch mode
        FILE* fp = fopen(argv[1], "r");
        if (fp == NULL) {
            perror("Error opening file");
            exit(1);
        }
        batch_mode(fp);
        fclose(fp);
    } else {
        // Interactive mode
        interactive_mode();
    }
    return 0;
}
