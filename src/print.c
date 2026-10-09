/*
 * print.c - all output formatting for ls.
 */
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "print.h"
#include "util.h"

/* Should odd characters be replaced by '?' right now? */
static int
use_question_marks(const options_t *opt)
{
    if (opt->qmode == QMODE_FORCE)
        return 1;
    if (opt->qmode == QMODE_RAW)
        return 0;
    return opt->tty_out;                /* default: -q on terminal, -w off */
}

void
print_filename(const char *name, const options_t *opt)
{
    int q = use_question_marks(opt);
    const unsigned char *p = (const unsigned char *)name;

    if (!q) {
        fputs(name, stdout);
        return;
    }
    for (; *p != '\0'; p++)
        putchar(isprint(*p) ? *p : '?');
}

void
print_header(const char *path, const options_t *opt)
{
    print_filename(path, opt);
    printf(":\n");
}

/*
 * Human readable sizes, modelled on humanize_number(3):
 * pick the largest unit that keeps the value < 1024, show one
 * decimal when the scaled value is < 10 (except for plain bytes).
 */
char *
humanize(long long bytes, char *buf, size_t bufsz)
{
    static const char units[] = "BKMGTPE";
    double v = (double)bytes;
    int u = 0;

    while (v >= 1024.0 && u < 6) {
        v /= 1024.0;
        u++;
    }
    if (u == 0)
        snprintf(buf, bufsz, "%lld", bytes);
    else if (v < 10.0)
        snprintf(buf, bufsz, "%.1f%c", v, units[u]);
    else
        snprintf(buf, bufsz, "%.0f%c", v, units[u]);
    return buf;
}

/* "Oct  9 07:40" - month abbrev, space-padded day, zero-padded HH:MM. */
void
format_date(time_t t, char *buf, size_t bufsz)
{
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    struct tm *tm = localtime(&t);      /* TZ is honoured via environ */

    if (tm == NULL) {
        snprintf(buf, bufsz, "???  ? ??:??");
        return;
    }
    snprintf(buf, bufsz, "%s %2d %02d:%02d",
        months[tm->tm_mon], tm->tm_mday, tm->tm_hour, tm->tm_min);
}

/* Number of display blocks for one file, honouring -h/-k/BLOCKSIZE. */
static long long
entry_blocks(const entry_t *e, const options_t *opt, char *human_buf,
    size_t human_sz)
{
    long long bytes = (long long)e->st.st_blocks * 512LL;

    if (opt->human)
        return (long long)strlen(humanize(bytes, human_buf, human_sz));
    /* round partial units up, like the manual page requires */
    return (bytes + opt->blocksize - 1) / opt->blocksize;
}

/* Number of decimal digits of an unsigned value (for column widths). */
static int
num_width(unsigned long long v)
{
    int w = 1;

    while (v >= 10) {
        v /= 10;
        w++;
    }
    return w;
}

/* Build e->sizestr for the long format and return its display width. */
static int
format_size(entry_t *e, const options_t *opt)
{
    if (S_ISCHR(e->st.st_mode) || S_ISBLK(e->st.st_mode)) {
        /* device files show "major, minor" in the size field */
        snprintf(e->sizestr, sizeof(e->sizestr), "%3u, %3u",
            major(e->st.st_rdev), minor(e->st.st_rdev));
    } else if (opt->human) {
        humanize((long long)e->st.st_size, e->sizestr, sizeof(e->sizestr));
    } else {
        snprintf(e->sizestr, sizeof(e->sizestr), "%lld",
            (long long)e->st.st_size);
    }
    return (int)strlen(e->sizestr);
}

static void
owner_name(const entry_t *e, const options_t *opt, char *buf, size_t bufsz)
{
    struct passwd *pw;

    if (!opt->numeric && (pw = getpwuid(e->st.st_uid)) != NULL)
        snprintf(buf, bufsz, "%s", pw->pw_name);
    else
        snprintf(buf, bufsz, "%u", e->st.st_uid);
}

static void
group_name(const entry_t *e, const options_t *opt, char *buf, size_t bufsz)
{
    struct group *gr;

    if (!opt->numeric && (gr = getgrgid(e->st.st_gid)) != NULL)
        snprintf(buf, bufsz, "%s", gr->gr_name);
    else
        snprintf(buf, bufsz, "%u", e->st.st_gid);
}

void
compute_widths(entry_t **items, size_t n, const options_t *opt, widths_t *w)
{
    size_t i;
    char tmp[64];

    memset(w, 0, sizeof(*w));
    for (i = 0; i < n; i++) {
        entry_t *e = items[i];
        int sw = format_size(e, opt);

        if (sw > w->sizew)
            w->sizew = sw;
        if (opt->longfmt) {
            int lw = num_width((unsigned long long)e->st.st_nlink);
            char user[64], group[64];

            if (lw > w->linkw)
                w->linkw = lw;
            owner_name(e, opt, user, sizeof(user));
            group_name(e, opt, group, sizeof(group));
            if ((int)strlen(user) > w->userw)
                w->userw = (int)strlen(user);
            if ((int)strlen(group) > w->groupw)
                w->groupw = (int)strlen(group);
        }
        if (opt->inode) {
            int iw = num_width((unsigned long long)e->st.st_ino);
            if (iw > w->inodew)
                w->inodew = iw;
        }
        if (opt->blocks) {
            long long b = entry_blocks(e, opt, tmp, sizeof(tmp));
            int bw = opt->human ? (int)b : num_width((unsigned long long)b);
            if (bw > w->blockw)
                w->blockw = bw;
        }
    }
}

void
print_total(entry_t **items, size_t n, const options_t *opt)
{
    unsigned long long total512 = 0;    /* st_blocks is in 512-byte units */
    size_t i;

    for (i = 0; i < n; i++)
        total512 += (unsigned long long)items[i]->st.st_blocks;
    if (opt->human) {
        char buf[32];
        humanize((long long)total512 * 512LL, buf, sizeof(buf));
        printf("total %s\n", buf);
    } else {
        unsigned long long shown =
            (total512 * 512ULL + (unsigned long long)opt->blocksize - 1ULL) /
            (unsigned long long)opt->blocksize;
        printf("total %llu\n", shown);
    }
}

/* The 10-character mode string from the manual page. */
static void
mode_string(mode_t mode, char *buf)
{
    /* entry type */
    if (S_ISDIR(mode))
        buf[0] = 'd';
    else if (S_ISLNK(mode))
        buf[0] = 'l';
    else if (S_ISCHR(mode))
        buf[0] = 'c';
    else if (S_ISBLK(mode))
        buf[0] = 'b';
    else if (S_ISFIFO(mode))
        buf[0] = 'p';
    else if (S_ISSOCK(mode))
        buf[0] = 's';
    else
        buf[0] = '-';

    /* owner / group / other permission triplets */
    for (int i = 0; i < 3; i++) {
        int shift = i * 3;
        buf[1 + i * 3] = (mode & (S_IRUSR >> shift)) ? 'r' : '-';
        buf[2 + i * 3] = (mode & (S_IWUSR >> shift)) ? 'w' : '-';
        buf[3 + i * 3] = (mode & (S_IXUSR >> shift)) ? 'x' : '-';
    }

    /* set-user-ID / set-group-ID / sticky, per the manual page */
    if (mode & S_ISUID)
        buf[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID)
        buf[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX)
        buf[9] = (mode & S_IXOTH) ? 't' : 'T';

    buf[10] = '\0';
}

/* -F type indicator for a name. */
static char
classify_char(const entry_t *e)
{
    mode_t m = e->st.st_mode;

    if (S_ISDIR(m))
        return '/';
    if (S_ISLNK(m))
        return '@';
    if (S_ISSOCK(m))
        return '=';
    if (S_ISFIFO(m))
        return '|';
    if (m & (S_IXUSR | S_IXGRP | S_IXOTH))
        return '*';
    return '\0';
}

/*
 * Print "name[/@*=|]".  In long format, symlinks also show " -> target".
 * (Plain "ls link" shows just the name, like the real ls.)
 */
static void
print_name_with_class(const entry_t *e, const options_t *opt, int longfmt)
{
    print_filename(e->name, opt);
    if (opt->classify) {
        char c = classify_char(e);
        if (c != '\0')
            putchar(c);
    }
    if (longfmt && e->is_symlink && e->link_target != NULL) {
        printf(" -> ");
        print_filename(e->link_target, opt);
    }
}

static void
print_long_entry(const entry_t *e, const widths_t *w, const options_t *opt)
{
    char mode[11];
    char user[64], group[64], date[32];
    struct timespec ts;

    mode_string(e->st.st_mode, mode);
    owner_name(e, opt, user, sizeof(user));
    group_name(e, opt, group, sizeof(group));

    switch (opt->timetype) {
    case TIME_CTIME: ts = e->st.st_ctim; break;
    case TIME_ATIME: ts = e->st.st_atim; break;
    default:         ts = e->st.st_mtim; break;
    }
    format_date(ts.tv_sec, date, sizeof(date));

    printf("%s %*lu %-*s %-*s %*s %s ",
        mode,
        w->linkw, (unsigned long)e->st.st_nlink,
        w->userw, user,
        w->groupw, group,
        w->sizew, e->sizestr,
        date);
    print_name_with_class(e, opt, 1);
    putchar('\n');
}

void
print_entry(const entry_t *e, const widths_t *w, const options_t *opt)
{
    if (opt->inode)
        printf("%*llu ", w->inodew, (unsigned long long)e->st.st_ino);
    if (opt->blocks) {
        if (opt->human) {
            char buf[32];
            humanize((long long)e->st.st_blocks * 512LL, buf, sizeof(buf));
            printf("%*s ", w->blockw, buf);
        } else {
            long long bytes = (long long)e->st.st_blocks * 512LL;
            long long shown = (bytes + opt->blocksize - 1) / opt->blocksize;
            printf("%*lld ", w->blockw, shown);
        }
    }
    if (opt->longfmt) {
        print_long_entry(e, w, opt);
    } else {
        print_name_with_class(e, opt, 0);
        putchar('\n');
    }
}
