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
#include <ctype.h>

int error = 0; //track the error

void process_line(char* line);
const char *search_paths[] = {
        "/usr/local/sbin/",
        "/usr/local/bin/",
        "/usr/sbin/",
        "/usr/bin/",
        "/sbin/",
        "/bin/",
        NULL
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



void interactive_mode() {
    write(STDOUT_FILENO, "Welcome to mysh!\n", 17);
    while (1) {
        
        if (error) {
            write(STDOUT_FILENO, "!mysh> ", 7);
            error = 0;
        } else {
            write(STDOUT_FILENO, "mysh> ", 6);
        }
       

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
            error = 1;
            exit(1);
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
    write(STDOUT_FILENO, "Goodbye!\n", 9);
}


char *find_command_path(const char *command) {
    struct stat sb;
    char *path = NULL;

    for (int i = 0; search_paths[i] != NULL; ++i) {
        path = malloc(strlen(search_paths[i]) + strlen(command) + 1);
        strcpy(path, search_paths[i]);
        strcat(path, command);

        if (stat(path, &sb) == 0 && sb.st_mode & S_IXUSR) {
            return path;
        }

        free(path);
        path = NULL;
    }

    return NULL;
}

void execute_command(char** args, int in_fd, int out_fd) {
    if (strcmp(args[0], "cd") == 0) {
        char *path = args[1];
        if (path == NULL) {
            path = getenv("HOME");
            if (path == NULL) {
                fprintf(stderr, "mysh: HOME environment variable not set\n");
                error = 1;
                return;
            }
        }
        if (chdir(path) != 0) {
            perror("cd");
            error = 1;
        }
        return;
    }

    if (strcmp(args[0], "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            fprintf(stdout, "%s\n", cwd);
        } else {
            perror("pwd");
            error = 1;
            
        }
        return;
    }
    
    if (strcmp(args[0], "exit") == 0) {
        exit(0); // terminate the program
    }
    
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        error = 1;
        
        return; // continue asking for input
    }

    if (pid == 0) {  // child process
        // Redirect input and output
        if (in_fd != STDIN_FILENO) {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }

        if (out_fd != STDOUT_FILENO) {
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }

        // Search for the command in the paths
        char *path = find_command_path(args[0]);

        if (path == NULL) {
            
            fprintf(stderr, "Command not found: %s\n", args[0]);
            error = 1;
            exit(1);
        }
        
        // Execute the command
        execv(path, args);

        // execv only returns if there was an error
        perror("execv");
        error = 1;
        exit(1); // continue asking for input
        
    }
    // parent process
    int status;
    wait(&status);
    
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        error = 1;
    }
   
    
}

void handle_wildcard(char* pattern, char** args, int* num_args) {
    char* dir_path = NULL;
    char* last_slash = strrchr(pattern, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
        dir_path = pattern;
        pattern = last_slash + 1;
    } else {
        dir_path = ".";
    }

    glob_t globbuf;
    int result = glob(pattern, GLOB_MARK, NULL, &globbuf);

    if (result != 0) {
        if (result == GLOB_NOMATCH) {
            globfree(&globbuf);
            return;
        }
        perror("Error opening directory");
        error = 1;
        return;
    }

    for (size_t i = 0; i < globbuf.gl_pathc; ++i) {
        char const *const matched_name = globbuf.gl_pathv[i];
        // skip directories and hidden files
        if ((matched_name[strlen(matched_name) - 1] != '/') && 
    (matched_name[strlen(dir_path)] != '.') &&
    (!(strstr(matched_name + strlen(dir_path), "/.") || strstr(matched_name + strlen(dir_path), "./")))) {
            args[*num_args] = strdup(matched_name); // In case of memory allocation errors
            (*num_args)++;
        }
    }

    globfree(&globbuf);
}

char* next_token(char** line) {
    char* token_start = NULL;
    while (**line) {
        if (!token_start && **line != ' ' && **line != '\t' && **line != '\n' && **line != '\r') {
            token_start = *line;
            if (**line == '<' || **line == '>' || **line == '|') {
                // If the token starts with a redirection operator, treat it as a single token
                (*line)++;
                return strndup(token_start, 1);
            }
        } else if (token_start && (**line == ' ' || **line == '\t' || **line == '\n' || **line == '\r' || **line == '<' || **line == '>' || **line == '|')) {
            // If the token ends with a redirection operator, treat it as a separate token
            if (**line == '<' || **line == '>' || **line == '|') {
                (*line)++;
                return strndup(*line - 1, 1);
            }
            **line = '\0';
            (*line)++;
            break;
        }
        (*line)++;
    }
    return token_start;
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
        return;
    }

    char* args[1024];
    int arg_index = 0;

    int in_fd = STDIN_FILENO;
    int out_fd = STDOUT_FILENO;
    int pipefd[2] = {-1, -1};

    char* token = next_token(&line);
    while (token != NULL) {
        if (strcmp(token, "<") == 0) {
            token = next_token(&line);
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
            token = next_token(&line);
            if (token == NULL) {
                fprintf(stderr, "Error: missing file after >\n");
                return;
            }
            if (out_fd != STDOUT_FILENO) {
                // If we already have an output file, close it first
                close(out_fd);
            }
            // Create a new output file
            out_fd = open(token, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd == -1) {
                perror("Error opening file for output");
                return;
            }
        } else if (strcmp(token, ">>") == 0) {
            token = next_token(&line);
            if (token == NULL) {
                fprintf(stderr, "Error: missing file after >>\n");
                return;
            }
            if (out_fd != STDOUT_FILENO) {
                // If we already have an output file, close it first
                close(out_fd);
            }
            // Open the output file in append mode
            out_fd = open(token, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (out_fd == -1) {
                perror("Error opening file for output");
                return;
            }
        } else if (strcmp(token, "|") == 0) {
            if (pipe(pipefd) == -1) {
                perror("Error creating pipe");
                return;
            }
            out_fd = pipefd[1];
            // Discard any remaining tokens on the command line
            while ((token = next_token(&line)) != NULL) {}
            break;
        } else {
            args[arg_index++] = token;
        }
        token = next_token(&line);
    }

    if (arg_index > 0) {
        args[arg_index] = NULL;
        execute_command(args, in_fd, out_fd);
    } else {
        // No command specified, just output an empty line to the file
        dprintf(out_fd, "\n");
    }

    if (in_fd != STDIN_FILENO) {
        close(in_fd);
    }
    if (out_fd != STDOUT_FILENO) {
        close(out_fd);
    }
    if (pipefd[0] != -1) {
        close(pipefd[0]);
    }
    if (pipefd[1] != -1) {
        close(pipefd[1]);
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
