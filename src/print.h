/*
 * print.h - displaying entries.
 *
 * Handles the plain one-per-line output, the -i/-s prefixes, the -F
 * type indicators, the -q/-w treatment of odd characters, and the
 * full long format (-l/-n) described in the manual page.
 */
#ifndef PRINT_H
#define PRINT_H

#include "entry.h"
#include "options.h"

/* Column widths for one directory listing (long format alignment). */
typedef struct {
    int linkw;                  /* number of links */
    int userw;                  /* owner name */
    int groupw;                 /* group name */
    int sizew;                  /* size (or "major, minor") */
    int inodew;                 /* inode number (-i) */
    int blockw;                 /* block count (-s) */
} widths_t;

/*
 * Compute column widths for a listing and fill each entry's sizestr.
 * Must be called before printing any entry of the listing.
 */
void compute_widths(entry_t **items, size_t n, const options_t *opt,
    widths_t *w);

/* Print the "total N" line shown before a listing with -l or -s. */
void print_total(entry_t **items, size_t n, const options_t *opt);

/* Print one entry: [inode] [blocks] [long format | name]. */
void print_entry(const entry_t *e, const widths_t *w, const options_t *opt);

/* Print a directory header ("path:"), applying -q/-w to the name. */
void print_header(const char *path, const options_t *opt);

/* Print a file name honouring -q/-w. */
void print_filename(const char *name, const options_t *opt);

/*
 * Format a byte count in human readable form ("512", "1.0K", "12M").
 * Returns buf.
 */
char *humanize(long long bytes, char *buf, size_t bufsz);

/* Format a timestamp as "Oct  9 07:40" (always HH:MM, like NetBSD). */
void format_date(time_t t, char *buf, size_t bufsz);

#endif /* PRINT_H */
