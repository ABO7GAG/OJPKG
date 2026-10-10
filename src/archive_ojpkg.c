#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <archive.h>
#include <archive_entry.h>
#include "archive_ojpkg.h"
#include "util.h"


/* Package paths are archive-relative. Reject absolute paths, dot components,
 * empty paths, and names too long for our buffers before prefixing dest_root. */
static int safe_relative_path(const char *path) {
    if (!path || !*path || path[0] == '/') return 0;
    if (strchr(path, '\\')) return 0;
    const char *part = path;
    for (const char *p = path; ; p++) {
        if (*p == '/' || *p == '\0') {
            size_t n = (size_t)(p - part);
            if (n == 0) {
                /* A single trailing slash is conventional for directory entries. */
                if (*p == '\0' && p > path && p[-1] == '/') break;
                return 0;
            }
            if ((n == 1 && part[0] == '.') ||
                (n == 2 && part[0] == '.' && part[1] == '.')) return 0;
            if (*p == '\0') break;
            part = p + 1;
        }
    }
    return 1;
}

static struct archive *open_pkg_archive(const char *archive_path) {
    struct archive *a = archive_read_new();
    archive_read_support_format_tar(a);
    archive_read_support_filter_gzip(a);
    archive_read_support_filter_xz(a); /* allow .tar.xz too */
    if (archive_read_open_filename(a, archive_path, 10240) != ARCHIVE_OK) {
        fprintf(stderr, "ojpkg: failed to open %s: %s\n", archive_path, archive_error_string(a));
        archive_read_free(a);
        return NULL;
    }
    return a;
}

int archive_read_pkginfo(const char *archive_path, ojpkg_t *out) {
    struct archive *a = open_pkg_archive(archive_path);
    if (!a) return -1;

    struct archive_entry *entry;
    int found = -1;
    char tmp[] = "/tmp/ojpkg_pkginfo_XXXXXX";
    int fd = mkstemp(tmp);
    if (fd < 0) { archive_read_free(a); return -1; }

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *name = archive_entry_pathname(entry);
        if (strcmp(name, ".PKGINFO") == 0) {
            FILE *out_f = fdopen(fd, "w");
            const void *buf; size_t size; la_int64_t offset;
            while (archive_read_data_block(a, &buf, &size, &offset) == ARCHIVE_OK)
                fwrite(buf, 1, size, out_f);
            fclose(out_f);
            found = pkg_parse_info(tmp, out);
            break;
        }
        archive_read_data_skip(a);
    }
    unlink(tmp);
    archive_read_free(a);
    return found;
}

char *archive_extract_install_script(const char *archive_path) {
    struct archive *a = open_pkg_archive(archive_path);
    if (!a) return NULL;

    struct archive_entry *entry;
    char *result = NULL;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *name = archive_entry_pathname(entry);
        if (strcmp(name, ".INSTALL") == 0) {
            char tmp[] = "/tmp/ojpkg_install_XXXXXX";
            int fd = mkstemp(tmp);
            if (fd < 0) break;
            FILE *out_f = fdopen(fd, "w");
            const void *buf; size_t size; la_int64_t offset;
            while (archive_read_data_block(a, &buf, &size, &offset) == ARCHIVE_OK)
                fwrite(buf, 1, size, out_f);
            fclose(out_f);
            chmod(tmp, 0755);
            result = strdup(tmp);
            break;
        }
        archive_read_data_skip(a);
    }
    archive_read_free(a);
    return result;
}

int archive_extract_pkg(const char *archive_path, const char *dest_root,
                         char ***files_out, int *n_out) {
    if (!files_out || !n_out || !dest_root || !*dest_root) return -1;
    *files_out = NULL;
    *n_out = 0;

    struct archive *a = open_pkg_archive(archive_path);
    if (!a) return -1;

    struct archive *ext = archive_write_disk_new();
    if (!ext) { archive_read_free(a); return -1; }
    archive_write_disk_set_options(ext,
        ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM |
        ARCHIVE_EXTRACT_ACL  | ARCHIVE_EXTRACT_FFLAGS |
        ARCHIVE_EXTRACT_SECURE_SYMLINKS | ARCHIVE_EXTRACT_SECURE_NODOTDOT);

    size_t capacity = 64;
    char **files = calloc(capacity, sizeof(*files));
    if (!files) {
        archive_write_free(ext); archive_read_free(a); return -1;
    }
    int n = 0, failed = 0;
    struct archive_entry *entry;
    int rv;
    while ((rv = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        const char *entry_name = archive_entry_pathname(entry);
        char relpath[4096];
        if (!entry_name || strlen(entry_name) >= sizeof(relpath)) {
            fprintf(stderr, "ojpkg: rejected overlong or missing archive path\n");
            failed = 1; break;
        }
        snprintf(relpath, sizeof(relpath), "%s", entry_name);
        if (strcmp(relpath, ".PKGINFO") == 0 || strcmp(relpath, ".INSTALL") == 0) {
            archive_read_data_skip(a);
            continue;
        }
        if (!safe_relative_path(relpath)) {
            fprintf(stderr, "ojpkg: rejected unsafe archive path: %s\n", relpath);
            failed = 1; break;
        }

        /* Keep the destination path bounded; libarchive's secure extraction
         * flags also reject dot-dot traversal and writes through symlinks. */
        char fullpath[8192];
        int written = snprintf(fullpath, sizeof(fullpath), "%s/%s", dest_root, relpath);
        if (written < 0 || (size_t)written >= sizeof(fullpath)) {
            fprintf(stderr, "ojpkg: destination path too long: %s\n", relpath);
            failed = 1; break;
        }
        archive_entry_set_pathname(entry, fullpath);

        int wh = archive_write_header(ext, entry);
        if (wh != ARCHIVE_OK) {
            fprintf(stderr, "ojpkg: extract header failed for %s: %s\n",
                    relpath, archive_error_string(ext));
            failed = 1; break;
        }
        const void *buf;
        size_t size;
        la_int64_t offset;
        while ((rv = archive_read_data_block(a, &buf, &size, &offset)) == ARCHIVE_OK) {
            if (archive_write_data_block(ext, buf, size, offset) != ARCHIVE_OK) {
                fprintf(stderr, "ojpkg: write failed for %s: %s\n", relpath,
                        archive_error_string(ext));
                failed = 1; break;
            }
        }
        if (rv != ARCHIVE_EOF && rv != ARCHIVE_OK) {
            fprintf(stderr, "ojpkg: archive data read failed for %s: %s\n",
                    relpath, archive_error_string(a));
            failed = 1;
        }
        if (archive_write_finish_entry(ext) != ARCHIVE_OK) {
            fprintf(stderr, "ojpkg: failed to finish %s: %s\n", relpath,
                    archive_error_string(ext));
            failed = 1;
        }
        if (failed) break;

        mode_t type = archive_entry_filetype(entry);
        if (type != AE_IFDIR) {
            if ((size_t)n == capacity) {
                size_t new_capacity = capacity * 2;
                char **grown = realloc(files, new_capacity * sizeof(*files));
                if (!grown) { failed = 1; break; }
                files = grown; capacity = new_capacity;
            }
            files[n] = strdup(relpath);
            if (!files[n]) { failed = 1; break; }
            n++;
        }
    }
    if (rv != ARCHIVE_EOF && !failed) {
        fprintf(stderr, "ojpkg: archive read error: %s\n", archive_error_string(a));
        failed = 1;
    }
    if (archive_write_close(ext) != ARCHIVE_OK) failed = 1;
    archive_write_free(ext);
    archive_read_free(a);

    if (failed) {
        for (int i = 0; i < n; i++) free(files[i]);
        free(files);
        return -1;
    }
    *files_out = files;
    *n_out = n;
    return 0;
}
