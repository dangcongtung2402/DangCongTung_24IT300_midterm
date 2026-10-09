/*
 * main.c - entry point: parse options, then list the operands.
 *
 * If no operands are given, the current directory is listed.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

#include "ls.h"
#include "options.h"

int
main(int argc, char **argv)
{
    options_t opt;
    int first_operand;
    char *dot[] = { "." };

    options_init(&opt);
    first_operand = parse_options(argc, argv, &opt);
    if (first_operand < 0)
        return 1;                       /* illegal option, usage shown */

    if (first_operand >= argc)
        return ls_run(dot, 1, &opt);    /* default: current directory */
    return ls_run(argv + first_operand, argc - first_operand, &opt);
}
