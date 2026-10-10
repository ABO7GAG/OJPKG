#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pkg.h"

void pkg_init(ojpkg_t *p) {
    memset(p, 0, sizeof(*p));
}

void pkg_free(ojpkg_t *p) {
    for (int i = 0; i < p->ndepends; i++) free(p->depends[i]);
    for (int i = 0; i < p->nprovides; i++) free(p->provides[i]);
    for (int i = 0; i < p->nconflicts; i++) free(p->conflicts[i]);
    for (int i = 0; i < p->nfiles; i++) free(p->files[i]);
    free(p->files);
    pkg_init(p);
}

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = 0;
    return s;
}

int pkg_parse_info(const char *path, ojpkg_t *p) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    pkg_init(p);

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = trim(line);
        char *val = trim(eq + 1);
        if (*key == '#' || *key == 0) continue;

        if (strcmp(key, "pkgname") == 0) snprintf(p->name, MAX_FIELD, "%s", val);
        else if (strcmp(key, "pkgver") == 0) snprintf(p->version, MAX_FIELD, "%s", val);
        else if (strcmp(key, "pkgdesc") == 0) snprintf(p->desc, MAX_FIELD, "%s", val);
        else if (strcmp(key, "arch") == 0) snprintf(p->arch, MAX_FIELD, "%s", val);
        else if (strcmp(key, "size") == 0) p->size = atol(val);
        else if (strcmp(key, "depend") == 0 && p->ndepends < MAX_LIST)
            p->depends[p->ndepends++] = strdup(val);
        else if (strcmp(key, "provides") == 0 && p->nprovides < MAX_LIST)
            p->provides[p->nprovides++] = strdup(val);
        else if (strcmp(key, "conflict") == 0 && p->nconflicts < MAX_LIST)
            p->conflicts[p->nconflicts++] = strdup(val);
        /* unknown keys are ignored on purpose -> forward compatible format */
    }
    fclose(f);
    return (p->name[0] && p->version[0]) ? 0 : -1;
}

int pkg_write_info(const char *path, const ojpkg_t *p) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "pkgname = %s\n", p->name);
    fprintf(f, "pkgver = %s\n", p->version);
    fprintf(f, "pkgdesc = %s\n", p->desc);
    fprintf(f, "arch = %s\n", p->arch);
    fprintf(f, "size = %ld\n", p->size);
    for (int i = 0; i < p->ndepends; i++) fprintf(f, "depend = %s\n", p->depends[i]);
    for (int i = 0; i < p->nprovides; i++) fprintf(f, "provides = %s\n", p->provides[i]);
    for (int i = 0; i < p->nconflicts; i++) fprintf(f, "conflict = %s\n", p->conflicts[i]);
    fclose(f);
    return 0;
}

int pkg_write_files(const char *path, char **files, int nfiles) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    for (int i = 0; i < nfiles; i++) fprintf(f, "%s\n", files[i]);
    fclose(f);
    return 0;
}

int pkg_read_files(const char *path, char ***files_out, int *n_out) {
    FILE *f = fopen(path, "r");
    if (!f) { *files_out = NULL; *n_out = 0; return -1; }
    char **arr = malloc(sizeof(char *) * 4096);
    int n = 0;
    char line[1024];
    while (fgets(line, sizeof(line), f) && n < 4096) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        if (line[0] == 0) continue;
        arr[n++] = strdup(line);
    }
    fclose(f);
    *files_out = arr;
    *n_out = n;
    return 0;
}

void pkg_print_short(const ojpkg_t *p) {
    printf("%s %s\n", p->name, p->version);
}

void pkg_print_info(const ojpkg_t *p) {
    printf("Name       : %s\n", p->name);
    printf("Version    : %s\n", p->version);
    printf("Description: %s\n", p->desc[0] ? p->desc : "(none)");
    printf("Arch       : %s\n", p->arch[0] ? p->arch : "any");
    printf("Size       : %ld bytes\n", p->size);
    printf("Depends    : ");
    if (p->ndepends == 0) printf("(none)");
    for (int i = 0; i < p->ndepends; i++) printf("%s%s", p->depends[i], i + 1 < p->ndepends ? ", " : "");
    printf("\n");
}

/* Compares versions segment by segment, splitting on '.' and '-'.
 * Numeric segments compare numerically, non-numeric compare lexically.
 * Good enough for a v1 resolver; not a full Debian/pacman-grade vercmp. */
int version_compare(const char *a, const char *b) {
    const char *pa = a, *pb = b;
    while (*pa || *pb) {
        while (*pa == '.' || *pa == '-') pa++;
        while (*pb == '.' || *pb == '-') pb++;

        char bufa[64], bufb[64];
        int ia = 0, ib = 0;
        while (*pa && *pa != '.' && *pa != '-' && ia < 63) bufa[ia++] = *pa++;
        while (*pb && *pb != '.' && *pb != '-' && ib < 63) bufb[ib++] = *pb++;
        bufa[ia] = 0; bufb[ib] = 0;

        if (ia == 0 && ib == 0) break;

        int numa = isdigit((unsigned char)bufa[0]);
        int numb = isdigit((unsigned char)bufb[0]);
        if (numa && numb) {
            long na = atol(bufa), nb = atol(bufb);
            if (na != nb) return na < nb ? -1 : 1;
        } else {
            int c = strcmp(bufa, bufb);
            if (c != 0) return c;
        }
    }
    return 0;
}
