We are implementing simple shell program for this project.
Goal of this project is to
    - In Interactive mode, read input commands from standard input and execute them.
    - In batch mode, read commands from a file and execute them.
    - Support some of the shell features like input/output redirection, handling wildcards, pipes, etc.
    - If theres any errors we use perror and exit(1).

Implementation - 
- We have two modes, Interactive and batch mode. As said in the goals, we get user input and execute the commands given. We have given directories in
search_paths array, program searches for the command in one of these directiries and executes them if it finds it. If no command exist, we perror
and exit(1). The process_line function is called each line of the input. It parses all items in the line and makes it into a list of arguments.
It handles all the tokens like input/output redirection, wildcard handling, pipes handling. The find_command_path function searches for the command
in the search_paths directories and returns a path to the executable file of the command if it does exist. The execute_command function forks a child process
to execute the command. It also includes the built-in functions like cd, pwd. This function used the find_command_path to find commands 
other than the built-in functions.

Extensions - 
3.2 Home directory
 - We implemented this extension which just changes working directory to home directory if user inputs cd without any arguments.



Testing - 
 - myscript.sh file can be used for testing the batch mode. The file includes different commands and when executed with mysh it
 executes all those commands in the file. Exits when we encounter "exit" command or EOF.


