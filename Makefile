all: mysh.c
	gcc -Wall -fsanitize=address -o mysh mysh.c

clean:
	$(RM) mysh