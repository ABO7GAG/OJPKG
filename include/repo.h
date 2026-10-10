#ifndef OJPKG_REPO_H
#define OJPKG_REPO_H

#include "pkg.h"

typedef struct {
    ojpkg_t meta;
    char archive_path[1024]; /* full path to the .ojpkg file */
} repo_entry_t;

typedef struct {
    repo_entry_t *entries; /* dynamically grown */
    int count;
    int capacity;
} repo_index_t;

/* Scans `dir` for *.ojpkg files and reads each one's .PKGINFO.
 * v1: local directories only. Network repos (http/https via libcurl) are
 * a planned phase-2 extension -- see README. */
int repo_scan(const char *dir, repo_index_t *idx);

/* Finds the newest matching entry by name across all given repo indexes. */
const repo_entry_t *repo_find(repo_index_t *indexes, int nindexes, const char *name);

void repo_free(repo_index_t *idx);

#endif
