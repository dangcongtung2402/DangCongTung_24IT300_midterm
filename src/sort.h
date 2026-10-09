/*
 * sort.h - ordering of entries.
 *
 * The manual page defines the ordering rules:
 *   default : lexicographical order of the file name
 *   -t      : most recently modified first (time from -c/-u applies),
 *             then lexicographical order as tie-break
 *   -S      : largest first, then lexicographical order as tie-break
 *   -f      : no sorting at all (directory order is kept)
 *   -r      : reverse the resulting order
 */
#ifndef SORT_H
#define SORT_H

#include "entry.h"
#include "options.h"

/* Sort the entries of a listing in place (no-op for SORT_NONE). */
void sort_entries(entry_t **items, size_t n, const options_t *opt);

#endif /* SORT_H */
