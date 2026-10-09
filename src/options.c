/*
 * options.c - parsing of the ls command line options.
 *
 * Only the option letters from the manual page are accepted:
 *   A a c d F f h i k l n q R r S s t u w
 * A "--" argument ends option parsing; a lone "-" is a file operand.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "options.h"
#include "util.h"

void
options_init(options_t *opt)
{
    const char *bs;

    memset(opt, 0, sizeof(*opt));
    opt->sort = SORT_NAME;
    opt->timetype = TIME_MTIME;
    opt->qmode = QMODE_AUTO;
    opt->blocksize = 512;               /* default unit: 512 bytes */
    opt->tty_out = isatty(STDOUT_FILENO);

    /* BLOCKSIZE affects -s counts and totals unless -h or -k is given. */
    bs = getenv("BLOCKSIZE");
    if (bs != NULL && *bs != '\0') {
        long v = parse_blocksize(bs);
        if (v > 0)
            opt->blocksize = v;
        else
            ls_warnx("ignoring invalid BLOCKSIZE value '%s'", bs);
    }
}

int
parse_options(int argc, char **argv, options_t *opt)
{
    int i, j;

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (arg[0] != '-' || arg[1] == '\0')
            break;                      /* not an option (or lone "-") */
        if (strcmp(arg, "--") == 0)
            return i + 1;               /* end of options */

        for (j = 1; arg[j] != '\0'; j++) {
            switch (arg[j]) {
            case 'A':
                opt->show_almost = 1;
                break;
            case 'a':
                opt->show_all = 1;
                break;
            case 'c':
                opt->timetype = TIME_CTIME;   /* overrides -u */
                break;
            case 'd':
                /* -R and -d override each other; last one wins. */
                opt->as_files = 1;
                opt->recursive = 0;
                break;
            case 'F':
                opt->classify = 1;
                break;
            case 'f':
                /* Only sorting is affected (per the manual page). */
                opt->sort = SORT_NONE;
                break;
            case 'h':
                /* -h and -k: the rightmost flag wins. */
                opt->human = 1;
                opt->kblocks = 0;
                break;
            case 'i':
                opt->inode = 1;
                break;
            case 'k':
                opt->human = 0;
                opt->kblocks = 1;
                break;
            case 'l':
                /* -l and -n: the last one wins. */
                opt->longfmt = 1;
                opt->numeric = 0;
                break;
            case 'n':
                opt->longfmt = 1;
                opt->numeric = 1;
                break;
            case 'q':
                /* -w and -q: the last one wins. */
                opt->qmode = QMODE_FORCE;
                break;
            case 'R':
                opt->recursive = 1;
                opt->as_files = 0;
                break;
            case 'r':
                opt->reverse = 1;
                break;
            case 'S':
                /* -t and -S: the last one wins. */
                opt->sort = SORT_SIZE;
                break;
            case 's':
                opt->blocks = 1;
                break;
            case 't':
                opt->sort = SORT_TIME;
                break;
            case 'u':
                opt->timetype = TIME_ATIME;   /* overrides -c */
                break;
            case 'w':
                opt->qmode = QMODE_RAW;
                break;
            default:
                fprintf(stderr, "ls: illegal option -- %c\n", arg[j]);
                fprintf(stderr,
                    "usage: ls [-AacdFfhiklnqRrSstuw] [file ...]\n");
                return -1;
            }
        }
    }

    /* Resolve the effective block unit for -s counts and totals. */
    if (opt->human) {
        /* human-readable: sizes are scaled per value, no fixed unit */
    } else if (opt->kblocks) {
        opt->blocksize = 1024;
    }
    /* otherwise: keep BLOCKSIZE (or the 512 default) from options_init */

    return i;
}
