#ifndef OJPKG_PKG_H
#define OJPKG_PKG_H

#define MAX_LIST 128
#define MAX_FIELD 256

typedef struct {
    char name[MAX_FIELD];
    char version[MAX_FIELD];     /* e.g. 1.2.3-1 (upstream-release) */
    char desc[MAX_FIELD];
    char arch[MAX_FIELD];
    long size;                   /* installed size in bytes */

    char *depends[MAX_LIST];
    int   ndepends;

    char *provides[MAX_LIST];
    int   nprovides;

    char *conflicts[MAX_LIST];
    int   nconflicts;

    char **files;                /* dynamically allocated; populated only
                                   * when loaded from the local db */
    int   nfiles;
} ojpkg_t;

void pkg_init(ojpkg_t *p);
void pkg_free(ojpkg_t *p);

/* Parse a .PKGINFO-style key=value file (as stored inside packages or the local db). */
int  pkg_parse_info(const char *path, ojpkg_t *p);

/* Write a .PKGINFO-style file from a populated struct (used when writing to local db). */
int  pkg_write_info(const char *path, const ojpkg_t *p);

/* Write the file-list (one relative path per line) for an installed package. */
int  pkg_write_files(const char *path, char **files, int nfiles);
int  pkg_read_files(const char *path, char ***files_out, int *n_out);

void pkg_print_short(const ojpkg_t *p);
void pkg_print_info(const ojpkg_t *p);

/* Naive dotted/segmented version compare: returns <0, 0, >0 like strcmp. */
int  version_compare(const char *a, const char *b);

#endif
