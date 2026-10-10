#ifndef OJPKG_UTIL_H
#define OJPKG_UTIL_H

/* mkdir -p equivalent. Returns 0 on success. */
int mkdir_p(const char *path, int mode);

/* Joins dir + "/" + name into buf (size bufsize). */
void path_join(char *buf, int bufsize, const char *dir, const char *name);

/* Returns 1 if path exists, 0 otherwise. */
int path_exists(const char *path);

#endif
