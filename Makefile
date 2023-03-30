all: mysh.c
	gcc -Wall -o mysh mysh.c

clean:
	$(RM) mysh