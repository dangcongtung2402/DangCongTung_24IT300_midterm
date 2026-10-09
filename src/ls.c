/*
 * ls.c - top-level listing logic.
 *
 * Flow:
 *   1. stat() every operand; failures are reported, the rest continues.
 *   2. non-directory operands are shown first, then directory operands;
 *      each group is sorted separately (lexicographically, or by -t/-S).
 *   3. each directory's contents are listed; -R recurses.
 *
 * A set of already-visited (device, inode) pairs guards -R against
 * infinite loops on directory cycles (e.g. bind mounts).
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "entry.h"
#include "ls.h"
#include "print.h"
#include "sort.h"
#include "util.h"

/* Directories already descended into (cycle guard for -R). */
typedef struct {
    dev_t dev;
    ino_t ino;
} visited_t;

typedef struct {
    visited_t *items;
    size_t n;
    size_t cap;
    const options_t *opt;
    int errors;                 /* any error seen so far */
    int listings_done;          /* blank line needed before next header? */
} ls_ctx_t;

static int
visited_contains(ls_ctx_t *ctx, dev_t dev, ino_t ino)
{
    size_t i;

    for (i = 0; i < ctx->n; i++)
        if (ctx->items[i].dev == dev && ctx->items[i].ino == ino)
            return 1;
    return 0;
}

static void
visited_add(ls_ctx_t *ctx, dev_t dev, ino_t ino)
{
    if (ctx->n == ctx->cap) {
        ctx->cap = ctx->cap ? ctx->cap * 2 : 32;
        ctx->items = xrealloc(ctx->items, ctx->cap * sizeof(*ctx->items));
    }
    ctx->items[ctx->n].dev = dev;
    ctx->items[ctx->n].ino = ino;
    ctx->n++;
}

/*
 * List the contents of one directory.
 * 'display' is the path shown in the "path:" header.
 */
static void
list_directory(ls_ctx_t *ctx, const char *path, const char *display,
    int show_header)
{
    entry_list_t *list;
    widths_t w;
    size_t i;

    list = read_directory(path, ctx->opt);
    if (list == NULL) {                 /* opendir failed, already reported */
        ctx->errors = 1;
        return;
    }

    sort_entries(list->items, list->n, ctx->opt);
    compute_widths(list->items, list->n, ctx->opt, &w);

    if (show_header) {
        if (ctx->listings_done)
            putchar('\n');              /* blank line between listings */
        print_header(display, ctx->opt);
    }
    ctx->listings_done = 1;

    if (ctx->opt->longfmt || ctx->opt->blocks)
        print_total(list->items, list->n, ctx->opt);
    for (i = 0; i < list->n; i++)
        print_entry(list->items[i], &w, ctx->opt);

    if (ctx->opt->recursive) {
        for (i = 0; i < list->n; i++) {
            entry_t *e = list->items[i];

            if (!S_ISDIR(e->st.st_mode)) /* never follow symlinks */
                continue;
            if (entry_is_dot_or_dotdot(e))
                continue;
            if (visited_contains(ctx, e->st.st_dev, e->st.st_ino)) {
                ls_warnx("%s: not listing already-listed directory",
                    e->path);
                ctx->errors = 1;
                continue;
            }
            visited_add(ctx, e->st.st_dev, e->st.st_ino);
            list_directory(ctx, e->path, e->path, 1);
        }
    }

    entry_list_free(list);
}

int
ls_run(char **operands, int noperands, const options_t *opt)
{
    ls_ctx_t ctx;
    entry_list_t files, dirs;
    size_t i;
    int show_header;

    memset(&ctx, 0, sizeof(ctx));
    ctx.opt = opt;
    memset(&files, 0, sizeof(files));
    memset(&dirs, 0, sizeof(dirs));

    /* 1. stat every operand; keep going after failures. */
    for (i = 0; i < (size_t)noperands; i++) {
        entry_t *e = entry_from_operand(operands[i], opt);

        if (e == NULL) {
            ctx.errors = 1;
            continue;
        }
        if (entry_is_listable_dir(e, opt))
            entry_list_add(&dirs, e);
        else
            entry_list_add(&files, e);
    }

    /* 2. non-directories first; each group sorted separately. */
    sort_entries(files.items, files.n, opt);
    sort_entries(dirs.items, dirs.n, opt);

    {
        widths_t w;

        compute_widths(files.items, files.n, opt, &w);
        for (i = 0; i < files.n; i++)
            print_entry(files.items[i], &w, opt);
        if (files.n > 0)
            ctx.listings_done = 1;
    }

    /*
     * 3. directory operands.  A "dir:" header is shown when there is
     * more than one operand, or when -R prints subdirectories anyway.
     */
    show_header = (noperands > 1) || opt->recursive;
    for (i = 0; i < dirs.n; i++) {
        entry_t *d = dirs.items[i];

        visited_add(&ctx, d->st.st_dev, d->st.st_ino);
        list_directory(&ctx, d->path, d->name, show_header);
    }

    for (i = 0; i < files.n; i++)
        entry_free(files.items[i]);
    for (i = 0; i < dirs.n; i++)
        entry_free(dirs.items[i]);
    free(files.items);
    free(dirs.items);
    free(ctx.items);

    return ctx.errors ? 1 : 0;
}
