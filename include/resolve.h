#ifndef OJPKG_RESOLVE_H
#define OJPKG_RESOLVE_H

#include "repo.h"

/* Resolves `name` and its full dependency chain against the given repo
 * indexes, skipping anything already installed under `root`.
 * Writes the install order (dependencies before dependents, no duplicates)
 * into queue[] (caller-provided array of const repo_entry_t*, length max).
 * Returns the number of entries queued, or -1 on unresolved dependency / cycle. */
int resolve_install_order(const char *root, repo_index_t *indexes, int nindexes,
                           const char *name, const repo_entry_t **queue, int max);

#endif
