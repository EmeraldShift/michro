#pragma once

struct fbuf_line {
	struct fbuf_line *next;
	char *s;
	int len;
};

struct fbuf {
	struct fbuf_line *lines;
	int nlines;
};

#define FBUF_INIT {NULL, 0}

extern int fbuf_load(char *fname, struct fbuf *buf);

struct fbuf_line *fbuf_getline(struct fbuf *buf, int nline);
