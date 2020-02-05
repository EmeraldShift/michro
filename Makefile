CFILES = $(wildcard *.c)
CFLAGS = -std=ansi -Wall -Werror -Wextra -pedantic

michro: $(CFILES)
	gcc -o michro $(CFILES)
