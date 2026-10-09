/*
 * ls.h - the top-level listing logic.
 */
#ifndef LS_H
#define LS_H

#include "options.h"

/*
 * Run ls on the given operands.  Returns the process exit status
 * (0 on success, 1 if any error occurred).
 */
int ls_run(char **operands, int noperands, const options_t *opt);

#endif /* LS_H */
