#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fnmatch.h>

void process_line(char* line);
const char *search_paths[] = {
        "/usr/local/sbin/",
        "/usr/local/bin/",
        "/usr/sbin/",
        "/usr/bin/",
        "/sbin/",
        "/bin/"
};

//batch mode seems to be working fine for now, will do more testing

void batch_mode(FILE *fp) {
    char line[1024];
    int index = 0;
    int c;

    while ((c = getc(fp)) != EOF) {
        if (c == '\n') {
            line[index] = '\0';

            if (strcmp(line, "exit") == 0) {
                break;
            }
            process_line(line);
            index = 0;
        } else {
            line[index++] = c;
            if (index >= sizeof(line)) {
                fprintf(stderr, "Error: input line too long\n");
                break;
            }
        }
    }
    if (index > 0) {
        line[index] = '\0';
        if (strcmp(line, "exit") != 0) {
            process_line(line);
        }
    }
}


//after doing pwd code exits itself, doesn't keep getting input.
//cd is also not changing directory. I believe we have to use chdir() to get the cd command working, cd takes one argument
//pwd works but code exits out. ---(1) error line  
//for pwd we have to use getcwd() and pwd takes no arguments
void interactive_mode() {
    printf("Welcome to mysh!\n");
    while (1) {
        printf("mysh> "); //we gotta use write() to print out mysh> at the same line of getting input (input would look like: mysh> pwd)

        char command[1024];
        int command_length = 0;
        char ch;
        ssize_t read_result;

        while ((read_result = read(STDIN_FILENO, &ch, 1)) > 0) {
            if (ch == '\n') {
                break;
            }
            command[command_length++] = ch;
        }

        if (read_result == -1) {
            perror("Error reading from stdin");  //---(1)
            break;
        }

        if (read_result == 0) { // EOF
            break;
        }

        command[command_length] = '\0';

        if (strcmp(command, "exit") == 0) {
            break;
        }

        process_line(command);
    }
    printf("Goodbye!\n");
}


char *find_command_path(const char *command) {
    struct stat sb;
    char *path = NULL;

    for (int i = 0; i < sizeof(search_paths) / sizeof(search_paths[0]); ++i) {
        path = malloc(strlen(search_paths[i]) + strlen(command) + 1);
        strcpy(path, search_paths[i]);
        strcat(path, command);

        if (stat(path, &sb) == 0 && sb.st_mode & S_IXUSR) {
            return path;
        }

        free(path);
    }

    return NULL;
}

void execute_command(char** args, int in_fd, int out_fd) {
    int i = 0;
    while (args[i] != NULL) {
        if (strcmp(args[i], "<") == 0) {
            args[i] = NULL;
            in_fd = open(args[i + 1], O_RDONLY);
            if (in_fd < 0) {
                perror("Error opening input file");
                exit(1);
            }
            i++;
        } else if (strcmp(args[i], ">") == 0) {
            args[i] = NULL;
            out_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP);
            if (out_fd < 0) {
                perror("Error opening output file");
                exit(1);
            }
            i++;
        }
        i++;
    }
    pid_t pid = fork();
    if (pid == 0) {
        if (in_fd != STDIN_FILENO) {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }
        if (out_fd != STDOUT_FILENO) {
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }
        char *command_path = find_command_path(args[0]);
        if (command_path != NULL) {
            execv(command_path, args);
            free(command_path);
        } else {
            execvp(args[0], args);
        }

        if (strchr(args[0], '/') != NULL) {
            execv(args[0], args);
        } else {
            execvp(args[0], args);
        }
        perror("Error executing command");
        exit(1);
    } else if (pid < 0) {
        perror("Error forking");
        exit(1);
    } else {
        close(in_fd);
        close(out_fd);
        waitpid(pid, NULL, 0);
    }
}

void handle_wildcard(char* pattern, char** args, int* num_args) {
    char* dir_path = ".";
    char* last_slash = strrchr(pattern, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
        dir_path = pattern;
        pattern = last_slash + 1;
    }

    DIR* dir = opendir(dir_path);
    if (dir == NULL) {
        perror("Error opening directory");
        return;
    }

    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        if (fnmatch(pattern, entry->d_name, 0) == 0 && !(pattern[0] == '*' && entry->d_name[0] == '.')) {
            char* matched_path;
            if (last_slash != NULL) {
                int len = snprintf(NULL, 0, "%s/%s", dir_path, entry->d_name);
                char* matched_path = malloc(len + 1);
                if (matched_path == NULL) {
                    perror("Memory allocation error");
                    return;
                }
                sprintf(matched_path, "%s/%s", dir_path, entry->d_name);
            } else {
                matched_path = strdup(entry->d_name);
            }
            args[*num_args] = matched_path;
            (*num_args)++;
        }
    }

    closedir(dir);
}

void process_line(char* line) {
    char* args[1024];
    int num_args = 0;
    int in_fd = STDIN_FILENO;
    int out_fd = STDOUT_FILENO;
    int pipe_fds[2] = {-1, -1};

    char* token = strtok(line, " ");
    while (token != NULL) {
        if (strcmp(token, "|") == 0) {
            if (pipe(pipe_fds) == -1) {
                perror("Error creating pipe");
                return;
            }
            out_fd = pipe_fds[1];
        } else if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                fprintf(stderr, "Error: missing file after <\n");
                return;
            }
            in_fd = open(token, O_RDONLY);
            if (in_fd == -1) {
                perror("Error opening file for input");
                return;
            }
        } else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                fprintf(stderr, "Error: missing file after >\n");
                return;
            }
            out_fd = open(token, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd == -1) {
                perror("Error opening file for output");
                return;
            }
        } else if (strchr(token, '*') != NULL) {
            handle_wildcard(token, args, &num_args);
        } else {
            args[num_args++] = token;
        }

        if (out_fd != STDOUT_FILENO) {
            args[num_args] = NULL;
            execute_command(args, in_fd, out_fd);
            num_args = 0;
            in_fd = pipe_fds[0];
            out_fd = STDOUT_FILENO;
            close(pipe_fds[1]);
        }

        token = strtok(NULL, " ");
    }

    if (num_args > 0) {
        args[num_args] = NULL;
        execute_command(args, in_fd, out_fd);
    }
}




//main func


int main(int argc, char** argv) {
    if (argc > 1) {
        int fd = open(argv[1], O_RDONLY);
        if (fd == -1) {
            perror("Error opening file");
            return 1;
        }
        // redirect standard input to the opened file descriptor
        dup2(fd, STDIN_FILENO);

        batch_mode(stdin);

        close(fd);      // close the opened file descriptor
    } else {
        interactive_mode();
    }

    return 0;
}