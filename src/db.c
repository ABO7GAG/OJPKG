#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include "db.h"
#include "util.h"

void db_dir(const char *root, char *out, int outsize) {
    /* avoid a double slash when root == "/" */
    if (strcmp(root, "/") == 0)
        snprintf(out, outsize, "/var/lib/ojpkg/local");
    else
        snprintf(out, outsize, "%s/var/lib/ojpkg/local", root);
}

static void entry_dir(const char *root, const char *name, const char *version, char *out, int outsize) {
    char base[2048];
    db_dir(root, base, sizeof(base));
    int n = snprintf(out, outsize, "%s/%s-%s", base, name, version);
    if (n < 0 || n >= outsize) out[0] = 0;
}

/* Locate the installed entry by reading its metadata, never by splitting the
 * directory name: both package names and versions may contain hyphens. If old
 * databases contain duplicate versions, select the highest version. */
static int find_entry_dir(const char *root, const char *name, char *out, int outsize) {
    char base[2048];
    db_dir(root, base, sizeof(base));
    DIR *d = opendir(base);
    if (!d) return -1;

    int found = -1;
    char best_version[MAX_FIELD] = "";
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char candidate[2300], desc[2500];
        path_join(candidate, sizeof(candidate), base, ent->d_name);
        struct stat st;
        if (stat(candidate, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        path_join(desc, sizeof(desc), candidate, "desc");
        ojpkg_t p;
        if (pkg_parse_info(desc, &p) != 0) continue;
        int match = strcmp(p.name, name) == 0;
        if (match && (found != 0 || version_compare(p.version, best_version) > 0)) {
            if (strlen(candidate) < (size_t)outsize) {
                snprintf(out, outsize, "%s", candidate);
                snprintf(best_version, sizeof(best_version), "%s", p.version);
                found = 0;
            }
        }
        pkg_free(&p);
    }
    closedir(d);
    return found;
}

static int remove_entry_dir(const char *dir) {
    char path[2500];
    path_join(path, sizeof(path), dir, "desc");
    if (unlink(path) != 0 && errno != ENOENT) return -1;
    path_join(path, sizeof(path), dir, "files");
    if (unlink(path) != 0 && errno != ENOENT) return -1;
    return rmdir(dir);
}

int db_is_installed(const char *root, const char *name, char *ver_out, int ver_out_size) {
    ojpkg_t p;
    if (db_load_package(root, name, &p) != 0) return 0;
    if (ver_out) snprintf(ver_out, ver_out_size, "%s", p.version);
    pkg_free(&p);
    return 1;
}

int db_add_package(const char *root, const ojpkg_t *p, char **files, int nfiles) {
    char base[2048], dir[4096];
    db_dir(root, base, sizeof(base));
    if (mkdir_p(base, 0755) != 0) return -1;
    entry_dir(root, p->name, p->version, dir, sizeof(dir));
    if (mkdir_p(dir, 0755) != 0) return -1;

    char desc_path[2300], files_path[2300];
    path_join(desc_path, sizeof(desc_path), dir, "desc");
    path_join(files_path, sizeof(files_path), dir, "files");
    if (pkg_write_info(desc_path, p) != 0 || pkg_write_files(files_path, files, nfiles) != 0)
        return -1;

    /* Keep one authoritative current entry per package. Historical package
     * snapshots will be added separately as part of the rollback design. */
    DIR *d = opendir(base);
    if (!d) return -1;
    struct dirent *ent;
    int cleanup_error = 0;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char candidate[2300], desc[2500];
        path_join(candidate, sizeof(candidate), base, ent->d_name);
        if (strcmp(candidate, dir) == 0) continue;
        struct stat st;
        if (stat(candidate, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        path_join(desc, sizeof(desc), candidate, "desc");
        ojpkg_t old;
        if (pkg_parse_info(desc, &old) == 0) {
            int same = strcmp(old.name, p->name) == 0;
            pkg_free(&old);
            if (same && remove_entry_dir(candidate) != 0) cleanup_error = 1;
        }
    }
    closedir(d);
    if (cleanup_error) fprintf(stderr, "ojpkg: warning: stale database entries remain for %s\n", p->name);
    return 0;
}

int db_remove_package(const char *root, const char *name) {
    char base[2048];
    db_dir(root, base, sizeof(base));
    DIR *d = opendir(base);
    if (!d) return -1;
    struct dirent *ent;
    int removed = 0, failed = 0;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char candidate[2300], desc[2500];
        path_join(candidate, sizeof(candidate), base, ent->d_name);
        struct stat st;
        if (stat(candidate, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        path_join(desc, sizeof(desc), candidate, "desc");
        ojpkg_t p;
        if (pkg_parse_info(desc, &p) != 0) continue;
        int same = strcmp(p.name, name) == 0;
        pkg_free(&p);
        if (same) {
            if (remove_entry_dir(candidate) == 0) removed++;
            else failed = 1;
        }
    }
    closedir(d);
    return (failed || !removed) ? -1 : 0;
}

int db_load_package(const char *root, const char *name, ojpkg_t *out) {
    char dir[4096];
    if (find_entry_dir(root, name, dir, sizeof(dir)) != 0) return -1;

    char desc_path[2300];
    path_join(desc_path, sizeof(desc_path), dir, "desc");
    if (pkg_parse_info(desc_path, out) != 0) return -1;

    char files_path[2300];
    path_join(files_path, sizeof(files_path), dir, "files");
    char **files = NULL; int n = 0;
    pkg_read_files(files_path, &files, &n);
    out->files = files;   /* ownership transfers to out; freed by pkg_free() */
    out->nfiles = n;
    return 0;
}

int db_list_installed(const char *root, char **out, int max) {
    char base[2048];
    db_dir(root, base, sizeof(base));
    DIR *d = opendir(base);
    if (!d) return 0;

    struct dirent *ent;
    int n = 0;
    while ((ent = readdir(d)) != NULL && n < max) {
        if (ent->d_name[0] == '.') continue;
        char candidate[2300], desc[2500];
        path_join(candidate, sizeof(candidate), base, ent->d_name);
        struct stat st;
        if (stat(candidate, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        path_join(desc, sizeof(desc), candidate, "desc");
        ojpkg_t p;
        if (pkg_parse_info(desc, &p) != 0) continue;
        int duplicate = 0;
        for (int i = 0; i < n; i++) if (strcmp(out[i], p.name) == 0) duplicate = 1;
        if (!duplicate) out[n++] = strdup(p.name);
        pkg_free(&p);
    }
    closedir(d);
    return n;
}

int db_has_dependents(const char *root, const char *name) {
    char *names[1024];
    int n = db_list_installed(root, names, 1024);
    int result = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(names[i], name) != 0) {
            ojpkg_t p;
            if (db_load_package(root, names[i], &p) == 0) {
                for (int j = 0; j < p.ndepends; j++) {
                    if (strcmp(p.depends[j], name) == 0) { result = 1; break; }
                }
                pkg_free(&p);
            }
        }
        free(names[i]);
        if (result) break;
    }
    return result;
}
