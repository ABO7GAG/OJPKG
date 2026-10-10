#ifndef OJPKG_DB_H
#define OJPKG_DB_H

#include "pkg.h"

/* Returns the db directory for a given install root, e.g.
 * root="/" -> "/var/lib/ojpkg/local"
 * root="/mnt/test" -> "/mnt/test/var/lib/ojpkg/local" */
void db_dir(const char *root, char *out, int outsize);

/* 1 if installed, 0 if not. If installed and ver_out is non-NULL, fills it. */
int  db_is_installed(const char *root, const char *name, char *ver_out, int ver_out_size);

/* Record an installed package: writes desc + files list under the db dir. */
int  db_add_package(const char *root, const ojpkg_t *p, char **files, int nfiles);

/* Remove a package's db entry (does NOT delete the installed files themselves). */
int  db_remove_package(const char *root, const char *name);

/* Load full metadata (including file list) for an installed package. */
int  db_load_package(const char *root, const char *name, ojpkg_t *out);

/* List all installed package names into out[] (caller-provided array of
 * char* of length max). Returns count. Caller frees each string. */
int  db_list_installed(const char *root, char **out, int max);

/* Returns 1 if any installed package depends on `name` (other than itself). */
int  db_has_dependents(const char *root, const char *name);

#endif
