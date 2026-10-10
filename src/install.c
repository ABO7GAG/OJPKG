#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "install.h"
#include "resolve.h"
#include "archive_ojpkg.h"
#include "db.h"
#include "util.h"

static void run_hook(const char *script_path, const char *stage) {
    if (!script_path) return;
    fflush(stdout); /* keep hook output in order relative to our own printf()s */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        ". \"%s\"; type %s >/dev/null 2>&1 && %s",
        script_path, stage, stage);
    int rv = system(cmd);
    if (rv != 0)
        fprintf(stderr, "ojpkg: warning: hook stage '%s' exited with status %d\n", stage, rv);
}

static int check_conflicts(const char *root, const ojpkg_t *p) {
    for (int i = 0; i < p->nconflicts; i++) {
        if (db_is_installed(root, p->conflicts[i], NULL, 0)) {
            fprintf(stderr, "ojpkg: '%s' conflicts with already-installed '%s'\n",
                    p->name, p->conflicts[i]);
            return -1;
        }
    }
    return 0;
}

int install_file(const char *root, const char *archive_path) {
    ojpkg_t meta;
    if (archive_read_pkginfo(archive_path, &meta) != 0) {
        fprintf(stderr, "ojpkg: %s: missing or invalid .PKGINFO\n", archive_path);
        return -1;
    }

    char installed_ver[MAX_FIELD];
    if (db_is_installed(root, meta.name, installed_ver, sizeof(installed_ver))) {
        if (version_compare(installed_ver, meta.version) >= 0) {
            printf("ojpkg: %s %s is already installed (>= %s), skipping\n",
                   meta.name, installed_ver, meta.version);
            pkg_free(&meta);
            return 0;
        }
        printf("ojpkg: upgrading %s %s -> %s\n", meta.name, installed_ver, meta.version);
    }

    if (check_conflicts(root, &meta) != 0) { pkg_free(&meta); return -1; }

    for (int i = 0; i < meta.ndepends; i++) {
        if (!db_is_installed(root, meta.depends[i], NULL, 0))
            fprintf(stderr, "ojpkg: warning: '%s' depends on '%s', which is not installed\n",
                    meta.name, meta.depends[i]);
    }

    if (mkdir_p(root, 0755) != 0) {
        fprintf(stderr, "ojpkg: cannot create root %s\n", root);
        pkg_free(&meta);
        return -1;
    }

    char *script = archive_extract_install_script(archive_path);
    run_hook(script, "pre_install");

    char **files = NULL; int nfiles = 0;
    if (archive_extract_pkg(archive_path, root, &files, &nfiles) != 0) {
        fprintf(stderr, "ojpkg: extraction failed for %s\n", archive_path);
        pkg_free(&meta);
        if (script) { unlink(script); free(script); }
        return -1;
    }

    if (db_add_package(root, &meta, files, nfiles) != 0) {
        fprintf(stderr, "ojpkg: warning: failed to record %s in local db\n", meta.name);
    }

    run_hook(script, "post_install");
    if (script) { unlink(script); free(script); }

    printf("ojpkg: installed %s %s (%d files)\n", meta.name, meta.version, nfiles);

    for (int i = 0; i < nfiles; i++) free(files[i]);
    free(files);
    pkg_free(&meta);
    return 0;
}

int install_by_name(const char *root, repo_index_t *indexes, int nindexes, const char *name) {
    const repo_entry_t *queue[1024];
    int n = resolve_install_order(root, indexes, nindexes, name, queue, 1024);
    if (n < 0) {
        fprintf(stderr, "ojpkg: cannot satisfy dependencies for '%s'\n", name);
        return -1;
    }
    if (n == 0) {
        printf("ojpkg: '%s' is already installed and up to date\n", name);
        return 0;
    }

    printf("ojpkg: resolved %d package(s) to install:\n", n);
    for (int i = 0; i < n; i++) printf("  %s %s\n", queue[i]->meta.name, queue[i]->meta.version);

    for (int i = 0; i < n; i++) {
        if (install_file(root, queue[i]->archive_path) != 0) {
            fprintf(stderr, "ojpkg: aborting, '%s' failed to install\n", queue[i]->meta.name);
            return -1;
        }
    }
    return 0;
}

int cmd_remove(const char *root, const char *name, int warn_only) {
    ojpkg_t p;
    if (db_load_package(root, name, &p) != 0) {
        fprintf(stderr, "ojpkg: '%s' is not installed\n", name);
        return -1;
    }

    if (db_has_dependents(root, name)) {
        fprintf(stderr, "ojpkg: refusing to remove '%s': other packages depend on it\n", name);
        if (!warn_only) { pkg_free(&p); return -1; }
    }

    /* Removal hooks would come from a cached copy of the original .INSTALL;
     * v1 does not keep that around, so pre_remove/post_remove are skipped
     * here and documented as a phase-2 improvement (store .INSTALL in db). */

    for (int i = 0; i < p.nfiles; i++) {
        char full[2048];
        if (strcmp(root, "/") == 0)
            snprintf(full, sizeof(full), "/%s", p.files[i]);
        else
            snprintf(full, sizeof(full), "%s/%s", root, p.files[i]);
        if (unlink(full) != 0) {
            /* might be a directory entry left behind on purpose; ignore */
        }
    }

    db_remove_package(root, name);
    printf("ojpkg: removed %s %s\n", p.name, p.version);
    pkg_free(&p);
    return 0;
}
