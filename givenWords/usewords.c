#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "words.h"

int main()
{
	char *word;
	
	words_init(STDIN_FILENO);
	
	while ((word = words_next())) {
		puts(word);
		free(word);
	}

	return EXIT_SUCCESS;
}
