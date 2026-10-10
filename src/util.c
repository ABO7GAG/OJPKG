#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "util.h"

int mkdir_p(const char *path, int mode) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0) { errno = EINVAL; return -1; }
    /* Preserve the filesystem root: stripping its slash turns "/" into "". */
    while (len > 1 && tmp[len - 1] == '/') tmp[--len] = 0;
    if (strcmp(tmp, "/") == 0) return 0;

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
    return 0;
}

void path_join(char *buf, int bufsize, const char *dir, const char *name) {
    snprintf(buf, bufsize, "%s/%s", dir, name);
}

int path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}
