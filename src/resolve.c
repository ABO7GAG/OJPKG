#include <stdio.h>
#include <string.h>
#include "resolve.h"
#include "db.h"

#define MAX_VISIT 1024

typedef enum { UNVISITED = 0, VISITING, DONE } visit_state_t;

typedef struct {
    char names[MAX_VISIT][MAX_FIELD];
    visit_state_t states[MAX_VISIT];
    int count;
} visit_set_t;

static int visit_index(visit_set_t *vs, const char *name) {
    for (int i = 0; i < vs->count; i++)
        if (strcmp(vs->names[i], name) == 0) return i;
    if (vs->count < MAX_VISIT) {
        snprintf(vs->names[vs->count], MAX_FIELD, "%s", name);
        vs->states[vs->count] = UNVISITED;
        return vs->count++;
    }
    return -1;
}

static int dfs(const char *root, repo_index_t *indexes, int nindexes,
                const char *name, visit_set_t *vs,
                const repo_entry_t **queue, int *qn, int max) {
    if (db_is_installed(root, name, NULL, 0)) return 0; /* already satisfied */

    int idx = visit_index(vs, name);
    if (idx < 0) return -1;

    if (vs->states[idx] == DONE) return 0;
    if (vs->states[idx] == VISITING) {
        fprintf(stderr, "ojpkg: dependency cycle detected at '%s'\n", name);
        return -1;
    }
    vs->states[idx] = VISITING;

    const repo_entry_t *e = repo_find(indexes, nindexes, name);
    if (!e) {
        fprintf(stderr, "ojpkg: unresolved dependency '%s' (not installed, not in any repo)\n", name);
        return -1;
    }

    for (int i = 0; i < e->meta.ndepends; i++) {
        if (dfs(root, indexes, nindexes, e->meta.depends[i], vs, queue, qn, max) != 0)
            return -1;
    }

    if (*qn >= max) {
        fprintf(stderr, "ojpkg: too many packages to resolve (limit %d)\n", max);
        return -1;
    }
    queue[(*qn)++] = e;
    vs->states[idx] = DONE;
    return 0;
}

int resolve_install_order(const char *root, repo_index_t *indexes, int nindexes,
                           const char *name, const repo_entry_t **queue, int max) {
    visit_set_t vs;
    vs.count = 0;
    int qn = 0;
    if (dfs(root, indexes, nindexes, name, &vs, queue, &qn, max) != 0)
        return -1;
    return qn;
}
