#ifndef OJPKG_INSTALL_H
#define OJPKG_INSTALL_H

#include "repo.h"

/* Installs a single .ojpkg archive file directly (no dependency resolution
 * beyond warning about missing deps). Returns 0 on success. */
int install_file(const char *root, const char *archive_path);

/* Resolves `name` + its dependency chain against the given repos and installs
 * everything in the right order. Returns 0 on success. */
int install_by_name(const char *root, repo_index_t *indexes, int nindexes, const char *name);

int cmd_remove(const char *root, const char *name, int cascade_warn_only);

#endif
