all: mysh

mysh: mysh.c list.c list.h parser.c wildcard.c io.h io.c
	gcc -g -Wall mysh.c list.c parser.c wildcard.c io.c -o $@

