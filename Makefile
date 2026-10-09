# Makefile for the ls(1) midterm project.
#
# Targets:
#   make          build the 'ls' binary
#   make test     build and run the test suite
#   make clean    remove the binary and object files

CC      = gcc
CFLAGS  = -std=c99 -D_GNU_SOURCE -Wall -Wextra -Werror -O2
SRCDIR  = src
TESTDIR = tests

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:.c=.o)
BIN  = ls

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(BIN)
	sh $(TESTDIR)/run_tests.sh ./$(BIN)

clean:
	rm -f $(BIN) $(OBJS)

.PHONY: all test clean
