#include "abuf.h"
#include "fbuf.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define CTRL_KEY(c) ((c) & 0x1f)

enum key {
	ARROW_UP = 1000,
	ARROW_DOWN,
	ARROW_RIGHT,
	ARROW_LEFT,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};

typedef struct {
	int cx, cy;
	int rows, cols;
	struct termios orig_termios;
	int dead;
	struct fbuf buffer;
	int topline;
	int clear;
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

int read_key(void)
{
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
		if (nread == -1 && errno != EAGAIN)
			die("read");

	if (c == '\x1b') {
		char seq[3];
		if (read(STDIN_FILENO, &seq[0], 1) != 1)
			return '\x1b';
		if (read(STDIN_FILENO, &seq[1], 1) != 1)
			return '\x1b';

		if (seq[0] == '[') {
			/* Page up/down */
			if (seq[1] >= '0' && seq[1] <= '9') {
				if (read(STDIN_FILENO, &seq[2], 1) != 1)
					return '\x1b';
				if (seq[2] == '~') {
					switch (seq[1]) {
					case '1':
						return HOME_KEY;
					case '2':
						return END_KEY;
					case '3':
						return DEL_KEY;
					case '5':
						return PAGE_UP;
					case '6':
						return PAGE_DOWN;
					case '7':
						return HOME_KEY;
					case '8':
						return END_KEY;
					}
				}
			}

			/* Arrow keys */
			switch(seq[1]) {
			case 'A':
				return ARROW_UP;
			case 'B':
				return ARROW_DOWN;
			case 'C':
				return ARROW_RIGHT;
			case 'D':
				return ARROW_LEFT;
			case 'H':
				return HOME_KEY;
			case 'F':
				return END_KEY;
			}
		}
		return '\x1b';
	}
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

void process_movement(int key)
{
	switch(key) {
	case ARROW_UP:
		if (E.cy > 0) {
			E.cy--;
		} else if (E.topline > 0) {
			E.topline--;
			E.clear = 1;
		}
		break;
	case ARROW_DOWN:
		if (E.cy < E.rows-1) {
			E.cy++;
		} else if (E.topline < E.buffer.nlines - 1) {
			E.topline++;
			E.clear = 1;
		}
		break;
	case ARROW_RIGHT:
		if (E.cx < E.cols-1)
			E.cx++;
		break;
	case ARROW_LEFT:
		if (E.cx > 0)
			E.cx--;
		break;
	case HOME_KEY:
		E.cx = 0;
		break;
	case END_KEY:
		E.cx = E.cols-1;
		break;
	case PAGE_UP:
	case PAGE_DOWN: {
		int rows = E.rows;
		while (rows--)
			process_movement(key == PAGE_UP ? ARROW_UP : ARROW_DOWN);
		break;
	}
	}
}

void process_key(int key)
{
	switch (key) {
	case CTRL_KEY('q'):
		exit(0);
		break;
	}
	process_movement(key);
}

int get_window_size(int *h, int *w)
{
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
			return -1;
		/* Cursor is in lower-right = win size */
		return get_cursor_pos(h, w);
	}
	*h = ws.ws_row;
	*w = ws.ws_col;
	return 0;
}

void draw_rows(struct abuf *ab)
{
	for (int y = 0; y < E.rows; y++) {
		if (y + E.topline < E.buffer.nlines) {
			struct fbuf_line *line = fbuf_getline(&E.buffer, y + E.topline);
			abuf_append(ab, line->s, line->len);
		} else {
			/* Only draw tildes after file end */
			abuf_append(ab, "~", 1);
			abuf_append(ab, "\x1b[K", 3);
		}
		if (y < E.rows-1)
			abuf_append(ab, "\r\n", 2);
	}
}

void refresh_screen(void)
{
	if (E.clear) {
		E.clear = 0;
		clear_screen();
	}
	struct abuf ab = ABUF_INIT;

	abuf_append(&ab, "\x1b[?25l", 6);
	abuf_append(&ab, "\x1b[H", 3);

	draw_rows(&ab);

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
	abuf_append(&ab, buf, strlen(buf));

	abuf_append(&ab, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ab.b, ab.len);
	abuf_free(&ab);
}

void init_editor(void)
{
	E.cx = E.cy = 0;
	if (get_window_size(&E.rows, &E.cols) == -1)
		die("get_window_size");
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "%s <file>\n", argv[0]);
		return 1;
	}

	atexit(clear_screen);
	enable_raw_mode();

	fbuf_load(argv[1], &E.buffer);

	init_editor();
	clear_screen();
	while (1) {
		refresh_screen();
		process_key(read_key());
	}
	return 0;
}
