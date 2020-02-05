#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#define CTRL_KEY(c) ((c) & 0x1f)

struct termios orig_termios;

void clear_screen(void)
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
}

void die(const char *s)
{
	clear_screen();
	perror(s);
	exit(1);
}

void disable_raw_mode(void)
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
		die("tcsetattr");
}

void enable_raw_mode(void)
{
	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
		die("tcgetattr");
	atexit(disable_raw_mode);

	struct termios raw = orig_termios;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag &= ~(CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
		die("tcsetattr");
}

char read_key(void)
{
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
		if (nread == -1 && errno != EAGAIN)
			die("read");
	return c;
}

void process_key(char c)
{
	switch (c) {
	case CTRL_KEY('q'):
		clear_screen();
		exit(0);
		break;
	}
}


int main(void)
{
	enable_raw_mode();
	while (1) {
		clear_screen();
		process_key(read_key());
	}
	return 0;
}
