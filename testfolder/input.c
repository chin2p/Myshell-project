#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_ARGS 10

int main(int argc, char *argv[]) {
    char *args[MAX_ARGS];
    char *input_file = NULL, *output_file = NULL;
    int i, in_fd, out_fd;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "<") == 0) {
            input_file = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], ">") == 0) {
            output_file = argv[i + 1];
            i++;
        } else {
            args[i - 1] = argv[i];
        }
    }

    args[i - 1] = NULL;

    if (input_file != NULL) {
        in_fd = open(input_file, O_RDONLY);
        if (in_fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        if (dup2(in_fd, STDIN_FILENO) == -1) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        close(in_fd);
    }

    if (output_file != NULL) {
        out_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (out_fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        if (dup2(out_fd, STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        close(out_fd);
    }

    execvp(args[0], args);
    perror("execvp");
    exit(EXIT_FAILURE);
}
