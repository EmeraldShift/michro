#pragma once

#include <string.h>

struct abuf {
	char *b;
	int len;
};

#define ABUF_INIT {NULL, 0}

extern void abuf_append(struct abuf *ab, const char *s, int len);

extern void abuf_free(struct abuf *ab);
