#ifndef OJPKG_ARCHIVE_H
#define OJPKG_ARCHIVE_H

#include "pkg.h"

/* Read just the .PKGINFO entry of a package archive into *out. Returns 0 on success. */
int archive_read_pkginfo(const char *archive_path, ojpkg_t *out);

/* Extract a package archive under dest_root, skipping .PKGINFO and .INSTALL.
 * On success, files_out and n_out receive the list of relative paths that
 * were written (caller frees each string + the array). Returns 0 on success. */
int archive_extract_pkg(const char *archive_path, const char *dest_root,
                         char ***files_out, int *n_out);

/* If the package archive contains a .INSTALL hook script, extract it to a
 * temp file and return its path (caller frees the returned string and should
 * unlink() the file when done). Returns NULL if no .INSTALL is present. */
char *archive_extract_install_script(const char *archive_path);

#endif
