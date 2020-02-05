#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define CTRL_KEY(c) ((c) & 0x1f)


typedef struct {
	int rows;
	int cols;
	struct termios orig_termios;
	int dead;
} EditorState;
EditorState E;


void clear_screen(void)
{
	/*
	 * dead=1 indicates that an error has occurred,
	 * and a message has been printed; Don't erase.
	 */
	if (!E.dead) {
		write(STDOUT_FILENO, "\x1b[2J", 4);
		write(STDOUT_FILENO, "\x1b[H", 3);
	}
}

void die(const char *s)
{
	clear_screen();
	E.dead = 1;
	perror(s);
	write(STDOUT_FILENO, "\r", 1);
	exit(1);
}

void disable_raw_mode(void)
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
		die("tcsetattr");
}

void enable_raw_mode(void)
{
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
		die("tcgetattr");
	atexit(disable_raw_mode);

	struct termios raw = E.orig_termios;
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

int get_cursor_pos(int *row, int *col)
{
	char buf[32];
	unsigned int i = 0;

	/* Request cursor location */
	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
		return -1;

	/* Read response into buf */
	while (i < sizeof(buf)-1) {
		if (read(STDIN_FILENO, &buf[i], 1) != 1)
			break;
		if (buf[i] == 'R')
			break;
		i++;
	}
	buf[i] = '\0';

	/* Parse response for row and col */
	if (buf[0] != '\x1b' && buf[1] != '[')
		return -1;
	if (sscanf(&buf[2], "%d;%d", row, col) != 2)
		return -1;
	return 0;
}

void process_key(char c)
{
	switch (c) {
	case CTRL_KEY('q'):
		exit(0);
		break;
	}
}

int get_window_size(int *w, int *h)
{
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
			return -1;
		/* Cursor is in lower-right = win size */
		return get_cursor_pos(w, h);
	}
	*w = ws.ws_col;
	*h = ws.ws_row;
	return 0;
}

void draw_rows(void)
{
	for (int y = 0; y < E.rows-1; y++)
		write(STDIN_FILENO, "~\r\n", 3);
	write(STDIN_FILENO, "~", 1);
}

void refresh_screen(void)
{
	clear_screen();
	draw_rows();
	write(STDOUT_FILENO, "\x1b[H", 3);
}

void init_editor(void)
{
	if (get_window_size(&E.rows, &E.cols) == -1)
		die("get_window_size");
}

int main(void)
{
	atexit(clear_screen);
	enable_raw_mode();
	init_editor();
	while (1) {
		refresh_screen();
		process_key(read_key());
	}
	return 0;
}
