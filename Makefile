CFILES = $(wildcard *.c)
CFLAGS = -g -std=c99 -Wall -Werror -Wextra -pedantic

michro: $(CFILES) Makefile
	gcc -o michro $(CFILES) $(CFLAGS)

-include *.d
