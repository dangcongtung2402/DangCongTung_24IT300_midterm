/*
 * entry.c - building entry lists from operands and directories.
 */
#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "entry.h"
#include "util.h"

/* Join dir + "/" + name into a freshly allocated path. */
static char *
join_path(const char *dir, const char *name)
{
    size_t dlen = strlen(dir);
    size_t nlen = strlen(name);
    int need_slash = (dlen > 0 && dir[dlen - 1] != '/');
    char *p = xmalloc(dlen + need_slash + nlen + 1);

    memcpy(p, dir, dlen);
    if (need_slash)
        p[dlen++] = '/';
    memcpy(p + dlen, name, nlen + 1);
    return p;
}

/* Read the target of a symlink into a freshly allocated string. */
static char *
read_link_target(const char *path)
{
    size_t cap = 128;
    char *buf;
    ssize_t n;

    for (;;) {
        buf = xmalloc(cap);
        n = readlink(path, buf, cap);
        if (n < 0) {                    /* not readable: not fatal */
            free(buf);
            return NULL;
        }
        if ((size_t)n < cap) {
            buf[n] = '\0';
            return buf;
        }
        /* Buffer too small: grow and retry. */
        free(buf);
        cap *= 2;
    }
}

/* Fill in the symlink-related fields of an entry. */
static void
entry_resolve_link(entry_t *e, const options_t *opt)
{
    struct stat tst;

    if (!e->is_symlink)
        return;
    e->link_target = read_link_target(e->path);

    /*
     * Without -d, a symlink in the argument list is "indirected through":
     * if it points at a directory we treat it like a directory.
     * Inside directory listings (-R) symlinks are never followed.
     */
    if (!opt->as_files && stat(e->path, &tst) == 0 && S_ISDIR(tst.st_mode))
        e->target_is_dir = 1;
}

static entry_t *
entry_new(const char *name, const char *path)
{
    entry_t *e = xmalloc(sizeof(*e));

    memset(e, 0, sizeof(*e));
    e->name = xstrdup(name);
    e->path = xstrdup(path);
    return e;
}

entry_t *
entry_from_operand(const char *operand, const options_t *opt)
{
    entry_t *e = entry_new(operand, operand);

    /* Operands are examined with lstat so that -d sees symlinks as links. */
    if (lstat(operand, &e->st) != 0) {
        ls_warn(operand);
        entry_free(e);
        return NULL;
    }
    e->is_symlink = S_ISLNK(e->st.st_mode) ? 1 : 0;
    entry_resolve_link(e, opt);
    return e;
}

int
entry_is_listable_dir(const entry_t *e, const options_t *opt)
{
    if (opt->as_files)
        return 0;                       /* -d: everything is a plain file */
    if (S_ISDIR(e->st.st_mode))
        return 1;
    return e->target_is_dir;            /* symlink -> dir on the cmdline */
}

int
entry_is_dot_or_dotdot(const entry_t *e)
{
    return e->name[0] == '.' &&
        (e->name[1] == '\0' ||
         (e->name[1] == '.' && e->name[2] == '\0'));
}

/*
 * Decide whether a directory entry is displayed:
 *   -a             -> everything, including "." and ".."
 *   -A (or root)   -> everything except "." and ".."
 *   default        -> skip names starting with '.'
 */
static int
entry_visible(const char *name, const options_t *opt)
{
    int is_dotfile = (name[0] == '.');

    if (!is_dotfile)
        return 1;
    if (opt->show_all)                  /* -a */
        return 1;
    if (name[1] == '\0' ||
        (name[1] == '.' && name[2] == '\0'))
        return 0;                       /* never show "." / ".." here */
    /* -A, always active for the super-user (see the manual page). */
    if (opt->show_almost || geteuid() == 0)
        return 1;
    return 0;
}

entry_list_t *
read_directory(const char *path, const options_t *opt)
{
    DIR *dp = opendir(path);
    entry_list_t *list;
    struct dirent *de;

    if (dp == NULL) {
        ls_warn(path);
        return NULL;
    }
    list = xmalloc(sizeof(*list));
    list->items = NULL;
    list->n = 0;
    list->cap = 0;

    while ((de = readdir(dp)) != NULL) {
        entry_t *e;
        char *full;

        if (!entry_visible(de->d_name, opt))
            continue;
        full = join_path(path, de->d_name);
        e = entry_new(de->d_name, full);
        free(full);
        /*
         * Entries inside a listing are examined with lstat: symlinks
         * stay symlinks (they are never followed by -R).
         */
        if (lstat(e->path, &e->st) != 0) {
            /* Entry vanished between readdir and lstat: skip it. */
            entry_free(e);
            continue;
        }
        e->is_symlink = S_ISLNK(e->st.st_mode) ? 1 : 0;
        if (e->is_symlink)
            e->link_target = read_link_target(e->path);
        entry_list_add(list, e);
    }
    closedir(dp);
    return list;
}

void
entry_list_add(entry_list_t *list, entry_t *e)
{
    if (list->n == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 32;
        list->items = xrealloc(list->items, list->cap * sizeof(*list->items));
    }
    list->items[list->n++] = e;
}

void
entry_free(entry_t *e)
{
    if (e == NULL)
        return;
    free(e->name);
    free(e->path);
    free(e->link_target);
    free(e);
}

void
entry_list_free(entry_list_t *list)
{
    size_t i;

    if (list == NULL)
        return;
    for (i = 0; i < list->n; i++)
        entry_free(list->items[i]);
    free(list->items);
    free(list);
}
