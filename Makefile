$(CC) = gcc -Wall -fsanitize=address, undefined -c

mysh: $(CC) mysh.c -o mysh

clean:
	rm *.o mysh