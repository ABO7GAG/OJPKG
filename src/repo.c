#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "repo.h"
#include "archive_ojpkg.h"

static int has_suffix(const char *s, const char *suffix) {
    size_t ls = strlen(s), lx = strlen(suffix);
    return ls >= lx && strcmp(s + ls - lx, suffix) == 0;
}

static void repo_grow(repo_index_t *idx) {
    if (idx->count < idx->capacity) return;
    idx->capacity = idx->capacity ? idx->capacity * 2 : 32;
    idx->entries = realloc(idx->entries, sizeof(repo_entry_t) * idx->capacity);
}

int repo_scan(const char *dir, repo_index_t *idx) {
    idx->count = 0;
    idx->capacity = 0;
    idx->entries = NULL;

    DIR *d = opendir(dir);
    if (!d) {
        fprintf(stderr, "ojpkg: cannot open repo dir %s\n", dir);
        return -1;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (!has_suffix(ent->d_name, ".ojpkg")) continue;

        repo_grow(idx);
        repo_entry_t *e = &idx->entries[idx->count];
        snprintf(e->archive_path, sizeof(e->archive_path), "%s/%s", dir, ent->d_name);
        if (archive_read_pkginfo(e->archive_path, &e->meta) == 0) {
            idx->count++;
        } else {
            fprintf(stderr, "ojpkg: warning: %s has no valid .PKGINFO, skipping\n", ent->d_name);
        }
    }
    closedir(d);
    return 0;
}

const repo_entry_t *repo_find(repo_index_t *indexes, int nindexes, const char *name) {
    const repo_entry_t *best = NULL;
    for (int i = 0; i < nindexes; i++) {
        for (int j = 0; j < indexes[i].count; j++) {
            repo_entry_t *e = &indexes[i].entries[j];
            if (strcmp(e->meta.name, name) == 0) {
                if (!best || version_compare(e->meta.version, best->meta.version) > 0)
                    best = e;
            }
        }
    }
    return best;
}

void repo_free(repo_index_t *idx) {
    for (int i = 0; i < idx->count; i++) pkg_free(&idx->entries[i].meta);
    free(idx->entries);
    idx->entries = NULL;
    idx->count = 0;
    idx->capacity = 0;
}
