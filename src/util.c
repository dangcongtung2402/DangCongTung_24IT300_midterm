/*
 * util.c - implementation of the helpers declared in util.h.
 */
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

void *
xmalloc(size_t size)
{
    void *p;

    /* malloc(0) may return NULL; force a non-zero request. */
    if (size == 0)
        size = 1;
    p = malloc(size);
    if (p == NULL) {
        fprintf(stderr, "ls: out of memory\n");
        exit(1);
    }
    return p;
}

void *
xrealloc(void *ptr, size_t size)
{
    void *p;

    if (size == 0)
        size = 1;
    p = realloc(ptr, size);
    if (p == NULL) {
        fprintf(stderr, "ls: out of memory\n");
        exit(1);
    }
    return p;
}

char *
xstrdup(const char *s)
{
    char *p = xmalloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
}

/* "ls: <path>: <strerror(errno)>" - used for failed system calls. */
void
ls_warn(const char *path)
{
    fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
}

/* "ls: <formatted message>" - for errors without an errno. */
void
ls_warnx(const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "ls: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

/*
 * Parse BLOCKSIZE values the way getbsize(3) does, in simplified form:
 * a decimal number optionally followed by one suffix:
 *   b = 512, k = 1024, m = 1024^2, g = 1024^3, t = 1024^4
 * (case-insensitive).  Returns bytes, or -1 on invalid input.
 */
long
parse_blocksize(const char *s)
{
    long value = 0;
    long mult = 1;
    const char *p = s;

    if (s == NULL || *s == '\0')
        return -1;
    while (isdigit((unsigned char)*p)) {
        value = value * 10 + (*p - '0');
        p++;
    }
    if (p == s)                 /* no digits at all */
        return -1;
    if (*p != '\0') {
        switch (tolower((unsigned char)*p)) {
        case 'b': mult = 512; break;
        case 'k': mult = 1024; break;
        case 'm': mult = 1024L * 1024; break;
        case 'g': mult = 1024L * 1024 * 1024; break;
        case 't': mult = 1024L * 1024 * 1024 * 1024; break;
        default: return -1;
        }
        p++;
        if (*p != '\0')         /* trailing garbage */
            return -1;
    }
    return value * mult;
}
