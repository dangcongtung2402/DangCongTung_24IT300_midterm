/*
 * entry.h - representation of one file system entry.
 *
 * An entry holds everything ls needs to sort and display a file:
 * its name, its lstat(2) data, and (for symlinks) where it points.
 */
#ifndef ENTRY_H
#define ENTRY_H

#include <sys/stat.h>
#include <stddef.h>

#include "options.h"

typedef struct entry {
    char *name;                 /* name as displayed (basename, or the
                                   operand string for command line args) */
    char *path;                 /* full path used for stat calls */
    struct stat st;             /* lstat() result */
    int is_symlink;             /* S_ISLNK(st.st_mode) */
    int target_is_dir;          /* symlink whose target is a directory */
    char *link_target;          /* readlink() result, NULL if not a link */
    char sizestr[32];           /* formatted size, filled by compute_widths */
} entry_t;

typedef struct {
    entry_t **items;
    size_t n;
    size_t cap;
} entry_list_t;

/* Build an entry for a command line operand.  Returns NULL on stat
 * failure (the error is reported to stderr). */
entry_t *entry_from_operand(const char *operand, const options_t *opt);

/* Read all displayable entries of directory 'path'.  On opendir()
 * failure returns NULL and reports the error. */
entry_list_t *read_directory(const char *path, const options_t *opt);

/* True if this entry should be listed as a directory's contents
 * (i.e. we descend into it), honouring -d. */
int entry_is_listable_dir(const entry_t *e, const options_t *opt);

/* True for "." and ".." (never recursed into). */
int entry_is_dot_or_dotdot(const entry_t *e);

void entry_free(entry_t *e);
void entry_list_free(entry_list_t *list);
void entry_list_add(entry_list_t *list, entry_t *e);

#endif /* ENTRY_H */
