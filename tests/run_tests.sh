#!/bin/sh
# Test suite for the ls(1) midterm implementation.
# Usage: sh tests/run_tests.sh [path-to-ls-binary]
#
# Builds a fixture tree under tests/tmp and checks the required
# behaviors from the manual page.  Exits 0 iff every test passes.

MYLS="${1:-./ls}"
case "$MYLS" in
/*) ;; # already absolute
*) MYLS="$(pwd)/$MYLS" ;;
esac
# Stage everything under /tmp: the unprivileged test user cannot
# traverse the project directory, so both the binary and the fixture
# tree must live somewhere world-accessible.
WORK=/tmp/ls-midterm-test
rm -rf "$WORK"
mkdir -p "$WORK"
cp "$MYLS" "$WORK/ls" || { echo "cannot stage test binary"; exit 1; }
MYLS="$WORK/ls"
TMP="$WORK/tree"
PASS=0
FAIL=0
unset BLOCKSIZE   # deterministic -s tests (one test sets it explicitly)

# --- helpers ---------------------------------------------------------------
pass() { PASS=$((PASS + 1)); printf 'ok   %s\n' "$1"; }
fail() { FAIL=$((FAIL + 1)); printf 'FAIL %s\n%s\n' "$1" "$2"; }

# check <desc> <expected> <actual>
check() {
    if [ "$2" = "$3" ]; then pass "$1"; else
        fail "$1" "--- expected ---\n$2\n--- actual ---\n$3"
    fi
}

# check_match <desc> <extended-regex> <actual>  (grep -Eq over the output)
check_match() {
    if printf '%s\n' "$3" | grep -Eq "$2"; then pass "$1"; else
        fail "$1" "--- output did not match /$2/ ---\n$3"
    fi
}

# Run as an unprivileged user when possible, so dotfile hiding and
# permission errors behave like they do for a normal user.
if [ "$(id -u)" -eq 0 ]; then
    as_nobody() { setpriv --reuid=nobody --regid=nogroup --clear-groups "$@"; }
    IS_ROOT=1
else
    as_nobody() { "$@"; }
    IS_ROOT=0
fi

# --- fixture tree ----------------------------------------------------------
STARTDIR=$(pwd)
rm -rf "$TMP"
mkdir -p "$TMP/dir1/sub" "$TMP/dir2" "$TMP/emptydir"
cd "$TMP" || exit 1

printf 'hello\n'       > a.txt
printf 'longer file\n' > b.txt
printf 'x'             > .hidden
printf 'sp ace\n'      > "sp ace.txt"
printf 'odd'           > "$(printf 'we\ttab')"
printf 'odd'           > "$(printf 'new\nline')"
printf '#!/bin/sh\n'   > runme.sh && chmod +x runme.sh
printf 'data'          > dir1/f1
printf 'moredata'      > dir1/f2
printf 'deep'          > dir1/sub/deep.txt
printf 'z'             > dir2/z1
ln -s a.txt      link_file
ln -s dir1       link_dir
ln -s /no/such   dangling
ln -s .          dir1/selfloop
ln a.txt         hardlink.txt
mkfifo           dir1/a_fifo
touch -d '2020-01-02 03:04:05' old.txt
touch -d '2026-06-07 08:09:10' new.txt
touch -d '2020-01-01 00:00:00' ctime_a ctime_b atime_a atime_b
touch -a -d '2026-01-01 00:00:00' atime_b
head -c 3000 /dev/zero | tr '\0' 'x' > bigfile   # for human-readable -l sizes
touch ./-dashfile
cd "$STARTDIR" || exit 1
chmod -R a+rX "$WORK"      # unprivileged user can read/traverse everything...
mkdir -p "$TMP/noperm/sub" && touch "$TMP/noperm/sub/f"
chmod 000 "$TMP/noperm"    # ...except this one
# real mode change AFTER the chmod -R above, so ctime_b sorts first with -ct
chmod 600 "$TMP/ctime_b"
T="$TMP"

# NOTE: file operands are displayed exactly as given on the command line,
# so expectations below use the "$T/..." form for file operands.

# ===========================================================================
# 1. plain listing: sorted, dotfiles hidden for non-root
# ===========================================================================
check "plain listing (non-root)" \
"$(cd $T && ls -1)" \
"$(as_nobody "$MYLS" "$T")"

if [ "$IS_ROOT" -eq 1 ]; then
    # manual page: -A is always set for the super-user
    check "root sees dotfiles without -a" \
    "$(cd $T && ls -A1)" \
    "$("$MYLS" "$T")"
fi

# ===========================================================================
# 2. -a / -A
# ===========================================================================
check "-a shows . and .." \
"$(cd $T && ls -a1)" \
"$(as_nobody "$MYLS" -a "$T")"

check "-A shows dotfiles but not . / .." \
"$(cd $T && ls -A1)" \
"$(as_nobody "$MYLS" -A "$T")"

# ===========================================================================
# 3. long format
# ===========================================================================
OUT=$(as_nobody "$MYLS" -l "$T/a.txt")
check_match "-l mode/owner/size/date/name" \
'^-rw[^ ]* +[0-9]+ +[a-z_][a-z0-9_-]* +[a-z_][a-z0-9_-]* +[0-9]+ +[A-Z][a-z]{2} +[0-9]+ +[0-9]{2}:[0-9]{2} +.*a\.txt$' \
"$OUT"

OUT=$(as_nobody "$MYLS" -l "$T/link_file")
check_match "-l symlink shows -> target" \
'^l.*link_file -> a\.txt$' "$OUT"

OUT=$(as_nobody "$MYLS" -ln "$T/a.txt")
case "$OUT" in *root*|*nobody*) fail "-n numeric check" "$OUT";; *) pass "-n has no names";; esac

OUT=$(as_nobody "$MYLS" -l "$T/hardlink.txt")
check_match "-l shows link count 2" '^[^ ]+ +2 +' "$OUT"

# -l and -n: last one wins
OUT=$(as_nobody "$MYLS" -ln -l "$T/a.txt")
case "$OUT" in *nobody*|*root*) pass "-ln -l ends with names";; *) fail "-ln -l ends with names" "$OUT";; esac
OUT=$(as_nobody "$MYLS" -l -n "$T/a.txt")
check_match "-l -n ends numeric" ' [0-9]+ +[0-9]+ +[0-9]+ ' "$OUT"

# total line present for -l and -s, even when empty
check_match "-l prints total line" '^total [0-9]+$' "$(as_nobody "$MYLS" -l "$T/emptydir")"
check_match "-s prints total line" '^total [0-9]+$' "$(as_nobody "$MYLS" -s "$T/emptydir")"
check "empty dir, plain: no output" "" "$(as_nobody "$MYLS" "$T/emptydir")"

# ===========================================================================
# 4. -i -s -h -k
# ===========================================================================
INO=$(stat -c %i "$T/a.txt")
check "-i prints inode" "$INO $T/a.txt" "$(as_nobody "$MYLS" -i "$T/a.txt")"

BLK=$(stat -c %b "$T/a.txt")   # 512-byte units, like our default
check "-s default 512-byte units" "$BLK $T/a.txt" "$(as_nobody "$MYLS" -s "$T/a.txt")"

check "-k forces kilobytes" \
"$(( (BLK * 512 + 1023) / 1024 )) $T/a.txt" \
"$(as_nobody "$MYLS" -sk "$T/a.txt")"

check "-h human readable -s" \
"4.0K $T/a.txt" \
"$(as_nobody "$MYLS" -sh "$T/a.txt")"

check "-h human readable -l size" \
"2.9K" \
"$(as_nobody "$MYLS" -lh "$T/bigfile" | awk '{print $5}')"

check "-k then -h: -h wins" \
"2.9K" \
"$(as_nobody "$MYLS" -lkh "$T/bigfile" | awk '{print $5}')"
check "-h then -k: -k wins (plain -l size)" \
"6" \
"$(as_nobody "$MYLS" -lhk "$T/a.txt" | awk '{print $5}')"

check "BLOCKSIZE=1k affects -s" \
"$(( (BLK * 512 + 1023) / 1024 )) $T/a.txt" \
"$(as_nobody env BLOCKSIZE=1k "$MYLS" -s "$T/a.txt")"

# ===========================================================================
# 5. -F indicators
# ===========================================================================
check "-F directory gets /"  "$T/dir1/"  "$(as_nobody "$MYLS" -F -d "$T/dir1")"
check "-F executable gets *" "$T/runme.sh*" "$(as_nobody "$MYLS" -F "$T/runme.sh")"
check "-F symlink gets @"    "$T/link_file@" "$(as_nobody "$MYLS" -F "$T/link_file")"
check "-F fifo gets |"        "$T/dir1/a_fifo|" "$(as_nobody "$MYLS" -F "$T/dir1/a_fifo")"
check "-F plain file unchanged" "$T/a.txt" "$(as_nobody "$MYLS" -F "$T/a.txt")"

# ===========================================================================
# 6. sorting: -t -S -r -f
# ===========================================================================
check "-t most recent first" \
"$T/new.txt
$T/old.txt" \
"$(as_nobody "$MYLS" -t "$T/new.txt" "$T/old.txt")"

check "-tr oldest first" \
"$T/old.txt
$T/new.txt" \
"$(as_nobody "$MYLS" -tr "$T/new.txt" "$T/old.txt")"

check "-S largest first" \
"$T/b.txt
$T/a.txt" \
"$(as_nobody "$MYLS" -S "$T/a.txt" "$T/b.txt")"

check "-r reverses names" \
"$(cd $T && ls -1r)" \
"$(as_nobody "$MYLS" -r "$T")"

check "-t matches system ls -t" \
"$(cd $T && ls -1t)" \
"$(as_nobody "$MYLS" -t "$T")"

check "-c sorts by status-change time" \
"$T/ctime_b
$T/ctime_a" \
"$(as_nobody "$MYLS" -ct "$T/ctime_a" "$T/ctime_b")"

check "-u sorts by access time" \
"$T/atime_b
$T/atime_a" \
"$(as_nobody "$MYLS" -ut "$T/atime_a" "$T/atime_b")"

# -f: no sorting -> same entry set as readdir order listing
N1=$(as_nobody "$MYLS" -f "$T/dir1" | sort | tr '\n' ' ')
N2=$(cd "$T/dir1" && ls -A1 | sort | tr '\n' ' ')
check "-f lists every entry exactly once" "$N2" "$N1"

# ===========================================================================
# 7. -R recursion
# ===========================================================================
OUT=$(as_nobody "$MYLS" -R "$T/dir1")
check_match "-R prints subdir header" "^$T/dir1/sub:$" "$OUT"
check_match "-R lists deep file" '^deep\.txt$' "$OUT"
case "$OUT" in *"$T/dir1/selfloop:"*) fail "-R descended into symlink loop" "$OUT";; *) pass "-R no symlink-loop descent";; esac

# -d and -R override each other; last one wins
check "-R -d lists dir as file" "$T/dir1" "$(as_nobody "$MYLS" -R -d "$T/dir1")"
check_match "-d -R recurses" "sub:" "$(as_nobody "$MYLS" -d -R "$T/dir1")"

# ===========================================================================
# 8. -d
# ===========================================================================
check "-d lists dir as plain file" "$T/dir1" "$(as_nobody "$MYLS" -d "$T/dir1")"
OUT=$(as_nobody "$MYLS" -ld "$T/dir1")
check_match "-ld shows the directory itself" '^d.*dir1$' "$OUT"
check "-d does not indirect symlinks" "$T/link_dir" "$(as_nobody "$MYLS" -d "$T/link_dir")"
check_match "-d -l shows symlink as link" 'link_dir -> dir1$' \
"$(as_nobody "$MYLS" -dl "$T/link_dir")"
# without -d, a symlink to a dir is descended into
check_match "symlink-to-dir operand lists contents" '^f1$' "$(as_nobody "$MYLS" "$T/link_dir")"

# ===========================================================================
# 9. operands: files first, headers, errors
# ===========================================================================
check "files before dirs, each sorted" \
"$T/a.txt
$T/b.txt

$T/dir1:
a_fifo
f1
f2
selfloop
sub" \
"$(as_nobody "$MYLS" "$T/b.txt" "$T/a.txt" "$T/dir1")"

check "single dir: no header" "z1" "$(as_nobody "$MYLS" "$T/dir2")"

OUT=$(as_nobody "$MYLS" "$T/nope" "$T/a.txt" 2>&1); CODE=$?
check_match "missing operand reported, others listed" 'a\.txt' "$OUT"
check_match "missing operand error message" 'ls: .*/nope: No such file or directory' "$OUT"
[ "$CODE" -eq 1 ] && pass "exit status 1 on error" || fail "exit status 1 on error" "got $CODE"

OUT=$(as_nobody "$MYLS" "$T/noperm" 2>&1); CODE=$?
check_match "unreadable dir reported" 'Permission denied' "$OUT"
[ "$CODE" -eq 1 ] && pass "exit status 1 on unreadable dir" || fail "exit status 1 on unreadable dir" "got $CODE"

check "dangling symlink does not crash" \
"$T/dangling" \
"$(as_nobody "$MYLS" "$T/dangling")"
# -lR over everything: must not crash (exit 1 is fine: $T/noperm is
# unreadable; only a signal-strength exit means a crash)
as_nobody "$MYLS" -lR "$T" >/dev/null 2>&1; CODE=$?
if [ "$CODE" -lt 128 ]; then pass "no crash on -lR everything (exit $CODE)"
else fail "no crash on -lR everything" "crashed with exit $CODE"; fi

# ===========================================================================
# 10. -q / -w
# ===========================================================================
N_Q=$(as_nobody "$MYLS" -q "$T" | grep -c '?')
[ "$N_Q" -ge 1 ] && pass "-q turns odd chars into ?" || fail "-q turns odd chars into ?" "no ? found"
# raw mode prints the embedded newline/tab bytes untouched
if as_nobody "$MYLS" -w "$T" | grep -q "$(printf 'we\ttab')"; then
    pass "-w prints raw odd bytes"
else
    fail "-w prints raw odd bytes" "tab name not found raw"
fi
# default when output is not a terminal is -w (raw)
if "$MYLS" "$T" | grep -q "$(printf 'we\ttab')"; then
    pass "default -w when not a terminal"
else
    fail "default -w when not a terminal" "expected raw name"
fi
# -q and -w: last one wins
if as_nobody "$MYLS" -qw "$T" | grep -q "$(printf 'we\ttab')"; then
    pass "-qw ends raw"
else
    fail "-qw ends raw" "expected raw name"
fi
if as_nobody "$MYLS" -wq "$T" | grep -q 'we?tab'; then
    pass "-wq ends with ?"
else
    fail "-wq ends with ?" "expected ? name"
fi

# ===========================================================================
# 11. option parsing edge cases
# ===========================================================================
"$MYLS" -Z "$T" >/dev/null 2>&1
[ $? -eq 1 ] && pass "illegal option exits 1" || fail "illegal option exits 1" "x"
check "-- ends option parsing" "$T/-dashfile" "$(as_nobody "$MYLS" -- "$T/-dashfile")"
check "no operands lists ." \
"$(cd $T/dir2 && ls -1)" \
"$(cd $T/dir2 && as_nobody "$MYLS")"

# ===========================================================================
# summary
# ===========================================================================
[ -z "$KEEP" ] && rm -rf "$WORK"
printf '\n%d passed, %d failed\n' "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ]
