#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>



void execute_command(char** args) {
    // commands here
    printf("Command executed: %s\n", args[0]);
}

void batch_mode(FILE* fp) {
    char line[1024];
    ssize_t num_read;

    while ((num_read = read(fileno(fp), line, sizeof(line)-1)) > 0) {
        // Null terminate the string and remove newline character
        line[num_read] = '\0';
        if (line[num_read-1] == '\n') {
            line[num_read-1] = '\0';
        }

        // Split the command into arguments
        char* args[1024];
        char* token = strtok(line, " ");
        int num_args = 0;
        while (token != NULL && num_args < 1023) {
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
        ssize_t num_read = read(STDIN_FILENO, command, 1024);
        if (num_read == -1) {
            perror("Error reading input");
            break;
        } else if (num_read == 0) {
            break;
        } else {
            command[num_read] = '\0';
        }

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
        int fd = open(argv[1], O_RDONLY);
        if (fd == -1) {
            perror("Error opening file");
            exit(1);
        }
        FILE* fp = fdopen(fd, "r");
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
