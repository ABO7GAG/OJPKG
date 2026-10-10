#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pkg.h"
#include "db.h"
#include "repo.h"
#include "install.h"
#include "archive_ojpkg.h"

#define MAX_REPOS 32

static void usage(const char *prog) {
    fprintf(stderr,
        "ojpkg - the OJLinux package manager\n\n"
        "Usage:\n"
        "  %s [--root PATH] [--repo DIR]... install <name|file.ojpkg>\n"
        "  %s [--root PATH] remove <name>\n"
        "  %s [--root PATH] query [name]        # list installed, or show one\n"
        "  %s [--root PATH] [--repo DIR]... search <keyword>\n"
        "  %s version\n\n"
        "Options:\n"
        "  --root PATH   install root (default: /)\n"
        "  --repo DIR     a local directory of .ojpkg files to use as a source\n"
        "                  (may be given multiple times)\n",
        prog, prog, prog, prog, prog);
}

static int has_suffix(const char *s, const char *suf) {
    size_t ls = strlen(s), lx = strlen(suf);
    return ls >= lx && strcmp(s + ls - lx, suf) == 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0); /* line-buffer so hook output interleaves correctly */
    const char *root = "/";
    char *repo_dirs[MAX_REPOS];
    int nrepos = 0;

    int i = 1;
    for (; i < argc; i++) {
        if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if (strcmp(argv[i], "--repo") == 0 && i + 1 < argc) {
            if (nrepos < MAX_REPOS) repo_dirs[nrepos++] = argv[++i];
        } else {
            break; /* first non-option argument = command */
        }
    }

    if (i >= argc) { usage(argv[0]); return 1; }
    const char *cmd = argv[i++];

    if (strcmp(cmd, "version") == 0) {
        printf("ojpkg 0.1.0 (OJLinux package manager)\n");
        return 0;
    }

    repo_index_t indexes[MAX_REPOS];
    for (int r = 0; r < nrepos; r++) repo_scan(repo_dirs[r], &indexes[r]);

    int status = 0;

    if (strcmp(cmd, "install") == 0) {
        if (i >= argc) { usage(argv[0]); return 1; }
        const char *target = argv[i];
        if (has_suffix(target, ".ojpkg"))
            status = install_file(root, target);
        else
            status = install_by_name(root, indexes, nrepos, target);

    } else if (strcmp(cmd, "remove") == 0) {
        if (i >= argc) { usage(argv[0]); return 1; }
        status = cmd_remove(root, argv[i], 0);

    } else if (strcmp(cmd, "query") == 0) {
        if (i < argc) {
            ojpkg_t p;
            if (db_load_package(root, argv[i], &p) == 0) {
                pkg_print_info(&p);
                pkg_free(&p);
            } else {
                fprintf(stderr, "ojpkg: '%s' is not installed\n", argv[i]);
                status = 1;
            }
        } else {
            char *names[4096];
            int n = db_list_installed(root, names, 4096);
            for (int k = 0; k < n; k++) {
                ojpkg_t p;
                if (db_load_package(root, names[k], &p) == 0) {
                    pkg_print_short(&p);
                    pkg_free(&p);
                }
                free(names[k]);
            }
        }

    } else if (strcmp(cmd, "search") == 0) {
        if (i >= argc) { usage(argv[0]); return 1; }
        const char *kw = argv[i];
        for (int r = 0; r < nrepos; r++) {
            for (int k = 0; k < indexes[r].count; k++) {
                ojpkg_t *m = &indexes[r].entries[k].meta;
                if (strstr(m->name, kw) || strstr(m->desc, kw))
                    printf("%s %s - %s\n", m->name, m->version, m->desc);
            }
        }

    } else {
        usage(argv[0]);
        status = 1;
    }

    for (int r = 0; r < nrepos; r++) repo_free(&indexes[r]);
    return status;
}
