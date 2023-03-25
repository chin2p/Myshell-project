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
#include <glob.h>

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
                puts("Error: input line too long");
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


//cd works but prints an error but it still works
//exit command is giving errors but it is working
//eof works fine
void interactive_mode() {
    write(STDOUT_FILENO, "Welcome to mysh!\n", 17);
    while (1) {
        write(STDOUT_FILENO, "mysh> ", 6);

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
            break;
        }

        if (read_result == 0) { // EOF
            break;
        }

        command[command_length] = '\0';

        if (strcmp(command, "exit") == 0) {
            exit(0);
        }

        process_line(command);
    }
    write(STDOUT_FILENO, "Goodbye!\n", 9);
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
            if (args[i + 1] == NULL) {
                fprintf(stderr, "Error: missing filename after <\n");
                return;
            }
            args[i] = NULL;
            in_fd = open(args[i + 1], O_RDONLY);
            if (in_fd < 0) {
                perror("Error opening input file");
                return;
            }
            i++;
        } else if (strcmp(args[i], ">") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "Error: missing filename after >\n");
                return;
            }
            args[i] = NULL;
            out_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP);
            if (out_fd < 0) {
                perror("Error opening output file");
                return;
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
        if (strcmp(args[0], "cd") == 0) {
            chdir(args[1]);
            return;
        }
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
        return;
    } else if (pid < 0) {
        perror("Error forking");
        return;
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

    glob_t globbuf;
    int result = glob(dir_path, GLOB_MARK | GLOB_BRACE | GLOB_TILDE | GLOB_NOCHECK | GLOB_NOESCAPE | GLOB_ERR,
                      NULL, &globbuf);

    if (result != 0 && result != GLOB_NOMATCH) {
        perror("Error opening directory");
        return;
    }

    for(size_t i=0; i<globbuf.gl_pathc; ++i){
        char const *const matched_name=globbuf.gl_pathv[i];
        //skip directories and hidden files
        if(matched_name[strlen(matched_name)-1]!='/'
           && !(pattern[0]== '*' && matched_name[strlen(dir_path)+1]=='.')){
            args[*num_args]=strdup(matched_name);//In case of memory allocation errors
            (*num_args)++;
        }
    }

    globfree(&globbuf);
}

void execute_echo(char** args) {
    // Check if there are any arguments to print
    if (args[1] == NULL) {
        printf("\n");
        return;
    }

    // Loop through each argument and print it
    for (int i = 1; args[i] != NULL; i++) {
        printf("%s ", args[i]);
    }
    printf("\n");
}

void process_line(char* line) {
    // Check if line is empty
    int len = strlen(line);
    int i;
    for (i = 0; i < len; i++) {
        char c = line[i];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
    }
    // Return error and ask for new input
    if (i == len) {
        puts("Error: no command entered");
        return;
    }
    char* args[1024];
    int num_args = 0;

    char* arg = strtok(line, " \t");
    while (arg != NULL) {
        args[num_args++] = arg;
        arg = strtok(NULL, " \t");
    }
    args[num_args] = NULL;

    // Handle special built-in commands
        if (num_args > 0 && strcmp(args[0], "echo") == 0) {
        execute_echo(args);
        return;
    }
    
if (strcmp(args[0], "cd") == 0) {
    if (num_args > 2) {
        fprintf(stderr, "Error: cd takes exactly one argument\n");
        return;
    }
    else if (num_args == 1){ // no argument provided
        if (chdir(getenv("HOME")) != 0) {
            perror("Error changing directory");
            return;
        }
        return;
    }
    if (chdir(args[1]) != 0) {
        perror("Error changing directory");
        return;
    }
}

if (strcmp(args[0], "pwd") == 0) {
    // Check if any arguments were provided
    if (num_args > 1) {
        fprintf(stderr, "Error: pwd does not accept arguments\n");
        return;
    }

    // Get current working directory and print it
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("Error getting current directory");
        return;
    }
    printf("%s\n", cwd);
    return;
}




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
