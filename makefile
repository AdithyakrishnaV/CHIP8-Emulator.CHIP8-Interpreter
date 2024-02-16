CFLAGS= -std=c17 -Wall -Werror -Wextra -lSDL2
all:
	gcc chip8.c -o  chip8 $(CFLAGS)