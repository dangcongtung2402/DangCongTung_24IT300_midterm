/*
 * sort.c - comparators implementing the manual page ordering rules.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>

#include "sort.h"

/* The options in effect while qsort runs (qsort gives us no context). */
static const options_t *sort_opt;

/* Pick the timestamp selected by -c/-u (default: mtime). */
static void
entry_time(const entry_t *e, struct timespec *ts)
{
    switch (sort_opt->timetype) {
    case TIME_CTIME:
        *ts = e->st.st_ctim;
        break;
    case TIME_ATIME:
        *ts = e->st.st_atim;
        break;
    default:
        *ts = e->st.st_mtim;
        break;
    }
}

static int
timespec_cmp(const struct timespec *a, const struct timespec *b)
{
    if (a->tv_sec != b->tv_sec)
        return (a->tv_sec > b->tv_sec) ? 1 : -1;
    if (a->tv_nsec != b->tv_nsec)
        return (a->tv_nsec > b->tv_nsec) ? 1 : -1;
    return 0;
}

/* Lexicographical order, honouring the locale like the real ls. */
static int
name_cmp(const entry_t *a, const entry_t *b)
{
    int r = strcoll(a->name, b->name);

    if (r == 0)                         /* deterministic tie-break */
        r = strcmp(a->name, b->name);
    return r;
}

static int
entry_cmp(const void *pa, const void *pb)
{
    const entry_t *a = *(const entry_t * const *)pa;
    const entry_t *b = *(const entry_t * const *)pb;
    int r = 0;

    switch (sort_opt->sort) {
    case SORT_TIME: {
        struct timespec ta, tb;
        entry_time(a, &ta);
        entry_time(b, &tb);
        r = timespec_cmp(&ta, &tb);
        r = -r;                         /* most recent first */
        if (r == 0)
            r = name_cmp(a, b);
        break;
    }
    case SORT_SIZE:
        if (a->st.st_size != b->st.st_size)
            r = (a->st.st_size > b->st.st_size) ? -1 : 1;  /* largest first */
        else
            r = name_cmp(a, b);
        break;
    case SORT_NAME:
    default:
        r = name_cmp(a, b);
        break;
    }

    if (sort_opt->reverse)
        r = -r;
    return r;
}

void
sort_entries(entry_t **items, size_t n, const options_t *opt)
{
    if (opt->sort == SORT_NONE || n < 2)
        return;                         /* -f: keep directory order */
    sort_opt = opt;
    qsort(items, n, sizeof(*items), entry_cmp);
    sort_opt = NULL;
}
