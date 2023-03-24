#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAX_INPUT_LENGTH 256

int main() {
  char input[MAX_INPUT_LENGTH];
  int n;

  while (1) {
    write(STDOUT_FILENO, "Input something here: ", 22);
    n = read(STDIN_FILENO, input, MAX_INPUT_LENGTH);

    // remove newline character from input
    input[strcspn(input, "\n")] = '\0';

    if (n == 0 || strncmp(input, "exit", 4) == 0) {
      write(STDOUT_FILENO, "\n", 1);
      break;
    }

    write(STDOUT_FILENO, "You entered: ", 13);
    write(STDOUT_FILENO, input, strlen(input));
    write(STDOUT_FILENO, "\n", 1);
  }

  return 0;
}
