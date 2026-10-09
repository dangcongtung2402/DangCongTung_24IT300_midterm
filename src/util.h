/*
 * util.h - small helpers shared by the whole program.
 *
 * Memory allocation wrappers abort on failure (ls must never continue
 * with a NULL pointer), plus error reporting and BLOCKSIZE parsing.
 */
#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

/* Allocate memory; terminate the program with an error message on failure. */
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);

/* Print "ls: <fmt>: <strerror>" style message to stderr. */
void ls_warn(const char *path);
void ls_warnx(const char *fmt, ...);

/*
 * Parse a BLOCKSIZE-style value ("512", "1k", "4M", ...).
 * Returns the size in bytes, or -1 if the string is invalid.
 */
long parse_blocksize(const char *s);

#endif /* UTIL_H */
