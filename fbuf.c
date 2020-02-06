#define _GNU_SOURCE
#include "fbuf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int fbuf_load(char *fname, struct fbuf *buf)
{
	FILE *fb;
	char *line = NULL;
	size_t len = 0;
	int nlines = 0;

	if ((fb = fopen(fname, "r")) == NULL)
		return -1;

	struct fbuf_line **ptr = &buf->lines;
	while (getline(&line, &len, fb) != -1) {
		*ptr = malloc(sizeof(struct fbuf_line));
		(*ptr)->next = NULL;
		(*ptr)->s = line;

		/*
		 * The size of the buffer is not necessarily the
		 * size of the string; compute it here.
		 */
		(*ptr)->len = strlen(((*ptr)->s))-1;
		ptr = &(*ptr)->next;
		nlines++;

		/*
		 * getline() will only create a new buffer
		 * if we pass in a null buffer, otherwise
		 * it will reuse it. So clear it now.
		 */
		line = NULL;
		len = 0;
	}
	buf->nlines = nlines;
	return 0;
}

struct fbuf_line *fbuf_getline(struct fbuf *buf, int nline)
{
	struct fbuf_line *line = buf->lines;
	int l = nline;
	while (l --> 0)
		line = line->next;
	return line;
}
