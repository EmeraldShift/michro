CFILES = $(wildcard *.c)
CFLAGS = -std=c99 -Wall -Werror -Wextra -pedantic

michro: $(CFILES)
	gcc -o michro $(CFILES) $(CFLAGS)
