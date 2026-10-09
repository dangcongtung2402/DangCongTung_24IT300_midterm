ĐẶNG CÔNG TÙNG - 24IT300 - 24JIT 



# Midterm Project — Implement `ls(1)`

A simplified, from-scratch implementation of the UNIX `ls(1)` command in C,
following the provided (NetBSD) manual page:
`ls [-AacdFfhiklnqRrSstuw] [file ...]`.

> **Student:** _Name_ — **Student ID:** _ID_ — **Class:** _class_
>
> **GitHub repository:** `https://github.com/<github-username>/Name_studentID_midterm`
> (rename the repository to `Name_studentID_midterm` with your real name and
> student ID before submitting; also update the URL above)

---

## 1. Implemented features

All option letters from the manual page are implemented:

| Option | Behavior |
|--------|----------|
| `-A` | List all entries except `.` and `..`. Always active for the super-user. |
| `-a` | Include entries whose names begin with `.` (even `.` and `..`). |
| `-c` | Use status-change time (`st_ctime`) instead of modification time for `-t` sorting and `-l` printing. |
| `-d` | List directories as plain files; symlinks in the argument list are not followed. |
| `-F` | Append `/` (dir), `*` (executable), `@` (symlink), `=` (socket), `\|` (FIFO). |
| `-f` | No sorting — entries stay in directory (`readdir`) order. |
| `-h` | Human-readable sizes for `-s` and `-l` (`4.0K`, `12M`, …). Overrides `-k`. |
| `-i` | Print the inode number before each entry. |
| `-k` | `-s` sizes in kilobytes. The rightmost of `-k`/`-h` wins. |
| `-l` | Long format: mode, links, owner, group, size, date, name. |
| `-n` | Like `-l`, but numeric UID/GID. |
| `-q` | Show non-printable characters as `?` (default when stdout is a terminal). |
| `-R` | Recursively list subdirectories. |
| `-r` | Reverse the sort order. |
| `-S` | Sort by size, largest first. |
| `-s` | Show blocks used (512-byte units, or `BLOCKSIZE`), rounded up; a `total` line precedes each listing. |
| `-t` | Sort by modification time, most recent first (name breaks ties). |
| `-u` | Use access time (`st_atime`) instead of modification time for `-t`/`-l`. |
| `-w` | Print non-printable characters raw (default when stdout is not a terminal). |

"Last one wins" overrides from the manual page are honored:
`-w`/`-q`, `-l`/`-n`, `-c`/`-u`, `-R`/`-d`, `-k`/`-h`, and `-t`/`-S`.

Other behaviors from the manual page:

- No operands → list `.`. Non-directory operands print first, then directories;
  each group is sorted separately (lexicographically, or by `-t`/`-S`).
- One entry per line; a `dir:` header precedes a listing when there are
  multiple operands (or under `-R`), with a blank line between listings.
- Long format shows `total N` (512-byte/`BLOCKSIZE` units) before each
  directory; symlinks show `name -> target`; device files show `major, minor`;
  dates always render as `Oct  9 07:40` (NetBSD style).
- `BLOCKSIZE` environment variable changes `-s` units when `-h`/`-k` are absent
  (suffixes `b/k/m/g/t` accepted, e.g. `BLOCKSIZE=1k`).
- Exit status is `0` on success, `1` if any error occurred; errors
  (`ls: path: reason`) go to stderr and never abort the remaining work.

### Robustness

- Every allocation is checked (`xmalloc`/`xrealloc`/`xstrdup`); every system
  call return value is handled — no unchecked `NULL` dereferences.
- `-R` never follows symlinks and keeps a visited `(device, inode)` set, so
  directory cycles (e.g. bind mounts, `ln -s . self`) cannot loop forever.
- Vanished entries, dangling symlinks, unreadable directories, empty
  directories, odd filenames (newlines, tabs, non-printable bytes) and bogus
  operands are all handled without crashing (verified with
  AddressSanitizer/UBSan — see §5).

## 2. How to build and run

Requirements: a C compiler (`gcc`) and `make`. No external libraries.

```sh
make            # builds the ./ls binary
./ls            # list the current directory
./ls -la /tmp   # long format, all entries, of /tmp
./ls -R -t src  # recursive, newest first
make test       # build (if needed) and run the 61-test suite
make clean      # remove the binary and object files
```

## 3. Project structure

```
.
├── Makefile        # build rules (all / test / clean)
├── README.md       # this report
├── .gitignore      # excludes the ls binary and *.o files
├── src/
│   ├── main.c      # entry point: parse options, dispatch to ls_run()
│   ├── options.h/.c# option parsing, defaults, BLOCKSIZE, tty detection
│   ├── entry.h/.c  # file entries: lstat, directory reading, dotfile rules
│   ├── sort.h/.c   # ordering: name / time / size / none, reverse
│   ├── print.h/.c  # output: -i/-s/-l/-F, mode string, dates, humanize
│   ├── ls.h/.c     # orchestration: operands, headers, recursion, cycles
│   └── util.h/.c   # checked allocators, error reporting, BLOCKSIZE parsing
└── tests/
    └── run_tests.sh# 61 behavior tests (see §5)
```

Each module owns one concern: `main` never touches the filesystem directly,
`entry` never prints, `print` never sorts, and so on.

## 4. Usage examples

```sh
$ ./ls
Makefile  README.md  src  tests

$ ./ls -l src
total 72
-rw-rw---- 1 root nogroup 2310 Oct  9 07:40 util.c
...

$ ./ls -F
Makefile  README.md  src/  tests/

$ ./ls -R -d src          # -d wins: directory shown as a plain file
src

$ ./ls -lt /tmp | head -3
total 12
drwxrwxrwt 2 root root 4096 Oct  9 07:40 tmp.abc
...
```

## 5. Testing

```sh
make test
```

`tests/run_tests.sh` builds a fixture tree (regular files, dotfiles, a
`sp ace` name, tab/newline names, an executable, a FIFO, hard/symlinks
including a dangling one and a symlink loop, an unreadable directory, files
with controlled mtime/atime/ctime) and runs **61 checks**:

- plain/`-a`/`-A` listing vs the system `ls`, incl. the super-user `-A` rule
- long format fields (mode string, link count, `-> target`, `-n` numerics,
  `-l`/`-n` override), `total` lines
- `-i`, `-s` (512/`-k`/`-h`/`BLOCKSIZE`), `-h`/`-k` override order
- `-F` indicators for dir/executable/symlink/FIFO
- `-t`/`-r`/`-S` ordering vs system `ls`, `-c`/`-u` time selection, `-f`
- `-R` recursion, headers, symlink-loop guard, `-R`/`-d` override
- `-d` (dirs as files, no symlink indirection), operand ordering, headers
- error paths: missing operand, unreadable dir (exit 1, others still listed)
- `-q`/`-w` defaults and overrides on odd filenames
- option parsing: illegal option, `--`, `-` , empty operands

Key tests run as an unprivileged user (`nobody` via `setpriv`) so dotfile
hiding and permission errors behave realistically. The whole suite also
passes under `-fsanitize=address,undefined` with no findings.

## 6. Notes and known differences

- `-f` follows the manual page literally ("output is not sorted"); unlike the
  real BSD `ls`, it does not imply `-a` nor disable `-l`/`-s`, because the
  manual page does not say so.
- Whiteouts (`-F` → `%`) and archive-state mode characters exist only on
  NetBSD and are not produced on Linux.
- Name sorting uses `strcoll` (locale-aware), like the real `ls`.
