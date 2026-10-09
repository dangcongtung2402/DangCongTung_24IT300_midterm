/*
 * options.h - command line option handling for ls.
 *
 * Mirrors the option set of the provided manual page:
 *   ls [-AacdFfhiklnqRrSstuw] [file ...]
 */
#ifndef OPTIONS_H
#define OPTIONS_H

/* Which timestamp -t sorting and -l printing use. */
typedef enum {
    TIME_MTIME,                 /* default: time of last modification */
    TIME_CTIME,                 /* -c: time of last status change */
    TIME_ATIME                  /* -u: time of last access */
} time_type_t;

/* How entries are ordered. */
typedef enum {
    SORT_NAME,                  /* default: lexicographical */
    SORT_TIME,                  /* -t: most recently modified first */
    SORT_SIZE,                  /* -S: largest first */
    SORT_NONE                   /* -f: directory order, no sorting */
} sort_mode_t;

/* How non-printable characters in file names are shown. */
typedef enum {
    QMODE_AUTO,                 /* -q on a terminal, -w otherwise */
    QMODE_FORCE,                /* -q: always print '?' */
    QMODE_RAW                   /* -w: always print raw bytes */
} qmode_t;

typedef struct {
    int show_all;               /* -a: include dotfiles (even "." and "..") */
    int show_almost;            /* -A: dotfiles except "." and ".." */
    int as_files;               /* -d: list dirs as plain files */
    int classify;               /* -F: append type indicator */
    sort_mode_t sort;           /* sorting mode (-f/-t/-S) */
    int human;                  /* -h: human readable sizes */
    int inode;                  /* -i: print inode numbers */
    int kblocks;                /* -k: -s sizes in kilobytes */
    int longfmt;                /* -l/-n: long format */
    int numeric;                /* -n: numeric uid/gid in long format */
    qmode_t qmode;              /* -q/-w handling of odd characters */
    int recursive;              /* -R: recurse into subdirectories */
    int reverse;                /* -r: reverse sort order */
    int blocks;                 /* -s: print block counts */
    time_type_t timetype;       /* -c/-u: which time to use */
    long blocksize;             /* unit for -s counts and totals */
    int tty_out;                /* is stdout a terminal? */
} options_t;

/* Fill opt with defaults (reads BLOCKSIZE and tty state). */
void options_init(options_t *opt);

/*
 * Parse argv options.  Returns the index of the first non-option
 * argument (the first file operand), or -1 on an invalid option.
 * Handles all "last one wins" overrides from the manual page.
 */
int parse_options(int argc, char **argv, options_t *opt);

#endif /* OPTIONS_H */
