#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "files_manager.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/statvfs.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static void set_err(char *err, size_t errn, const char *what) {
    (void)what;
    if (!err || errn == 0) return;
    snprintf(err, errn, "%s", strerror(errno));
}

static void set_text_err(char *err, size_t errn, const char *text) {
    if (!err || errn == 0) return;
    snprintf(err, errn, "%s", text ? text : "file operation failed");
}

static FmEntryType entry_type_from_mode(mode_t mode) {
    if (S_ISDIR(mode)) return FM_ENTRY_DIRECTORY;
    if (S_ISREG(mode)) return FM_ENTRY_REGULAR;
    if (S_ISLNK(mode)) return FM_ENTRY_SYMLINK;
    return FM_ENTRY_OTHER;
}

static int entry_cmp(const void *a, const void *b) {
    const FmEntry *ea = (const FmEntry *)a;
    const FmEntry *eb = (const FmEntry *)b;
    int ad = ea->type == FM_ENTRY_DIRECTORY;
    int bd = eb->type == FM_ENTRY_DIRECTORY;
    if (ad != bd) return bd - ad;
    int ci = strcasecmp(ea->name, eb->name);
    return ci ? ci : strcmp(ea->name, eb->name);
}

void fm_free_directory(FmDirectory *dir) {
    if (!dir) return;
    for (size_t i = 0; i < dir->count; ++i) free(dir->entries[i].name);
    free(dir->entries);
    dir->entries = NULL;
    dir->count = 0;
}

static int mount_field_unescape(const char *src, char **out) {
    size_t n = strlen(src);
    char *dst = malloc(n + 1);
    if (!dst) return -1;
    size_t u = 0;
    for (size_t i = 0; i < n; ) {
        if (src[i] == '\\' && i + 3 < n &&
            src[i + 1] >= '0' && src[i + 1] <= '7' &&
            src[i + 2] >= '0' && src[i + 2] <= '7' &&
            src[i + 3] >= '0' && src[i + 3] <= '7') {
            unsigned value = (unsigned)(src[i + 1] - '0') * 64u +
                             (unsigned)(src[i + 2] - '0') * 8u +
                             (unsigned)(src[i + 3] - '0');
            dst[u++] = (char)value;
            i += 4;
        } else {
            dst[u++] = src[i++];
        }
    }
    dst[u] = '\0';
    *out = dst;
    return 0;
}

static int fs_type_is_network(const char *fs) {
    static const char *network[] = {
        "nfs", "nfs4", "cifs", "smb3", "9p", "ceph", "glusterfs",
        "fuse.sshfs", "sshfs", "davfs", "fuse.davfs"
    };
    for (size_t i = 0; i < sizeof(network) / sizeof(network[0]); ++i)
        if (strcmp(fs, network[i]) == 0) return 1;
    return 0;
}

static int fs_type_is_pseudo(const char *fs) {
    static const char *pseudo[] = {
        "proc", "sysfs", "devtmpfs", "devpts", "tmpfs", "ramfs",
        "cgroup", "cgroup2", "pstore", "securityfs", "debugfs", "tracefs",
        "configfs", "hugetlbfs", "mqueue", "autofs", "fusectl", "binfmt_misc",
        "rpc_pipefs", "nsfs", "efivarfs"
    };
    for (size_t i = 0; i < sizeof(pseudo) / sizeof(pseudo[0]); ++i)
        if (strcmp(fs, pseudo[i]) == 0) return 1;
    return 0;
}

static int mount_options_read_only(const char *options) {
    if (!options) return 0;
    const char *p = options;
    while (*p) {
        const char *end = strchr(p, ',');
        size_t n = end ? (size_t)(end - p) : strlen(p);
        if (n == 2 && p[0] == 'r' && p[1] == 'o') return 1;
        if (!end) break;
        p = end + 1;
    }
    return 0;
}

static int location_has_mount(const FmLocations *locations, const char *mount_point) {
    for (size_t i = 0; i < locations->count; ++i)
        if (strcmp(locations->items[i].mount_point, mount_point) == 0) return 1;
    return 0;
}

static void free_location(FmLocation *location) {
    if (!location) return;
    free(location->mount_point);
    free(location->source);
    free(location->fs_type);
    memset(location, 0, sizeof(*location));
}

void fm_free_locations(FmLocations *locations) {
    if (!locations) return;
    for (size_t i = 0; i < locations->count; ++i) free_location(&locations->items[i]);
    free(locations->items);
    locations->items = NULL;
    locations->count = 0;
}

static int location_cmp(const void *a, const void *b) {
    const FmLocation *la = (const FmLocation *)a;
    const FmLocation *lb = (const FmLocation *)b;
    int ar = strcmp(la->mount_point, "/") == 0;
    int br = strcmp(lb->mount_point, "/") == 0;
    if (ar != br) return br - ar;
    if (la->kind != lb->kind) return (int)la->kind - (int)lb->kind;
    return strcmp(la->mount_point, lb->mount_point);
}

int fm_load_locations(const char *mountinfo_path, int show_system, FmLocations *out, char *err, size_t errn) {
    if (!out) { errno = EINVAL; set_err(err, errn, "locations"); return -1; }
    out->items = NULL;
    out->count = 0;
    if (!mountinfo_path || !*mountinfo_path) mountinfo_path = "/proc/self/mountinfo";
    FILE *f = fopen(mountinfo_path, "r");
    if (!f) { set_err(err, errn, mountinfo_path); return -1; }

    char *line = NULL;
    size_t line_cap = 0, cap = 0;
    int rc = 0, saved = 0;
    while (getline(&line, &line_cap, f) >= 0) {
        char *fields[128];
        size_t field_count = 0;
        char *save = NULL;
        for (char *tok = strtok_r(line, " \t\r\n", &save);
             tok && field_count < sizeof(fields) / sizeof(fields[0]);
             tok = strtok_r(NULL, " \t\r\n", &save)) fields[field_count++] = tok;
        if (field_count < 10) continue;
        size_t dash = 6;
        while (dash < field_count && strcmp(fields[dash], "-") != 0) dash++;
        if (dash + 2 >= field_count || dash == field_count) continue;

        char *mount_point = NULL, *source = NULL, *fs_type = NULL;
        if (mount_field_unescape(fields[4], &mount_point) != 0 ||
            mount_field_unescape(fields[dash + 2], &source) != 0 ||
            mount_field_unescape(fields[dash + 1], &fs_type) != 0) {
            saved = ENOMEM; free(mount_point); free(source); free(fs_type); rc = -1; break;
        }
        int is_root = strcmp(mount_point, "/") == 0;
        struct stat mount_st;
        if (stat(mount_point, &mount_st) != 0 || !S_ISDIR(mount_st.st_mode)) {
            free(mount_point); free(source); free(fs_type); continue;
        }
        int pseudo = fs_type_is_pseudo(fs_type);
        if ((!show_system && pseudo && !is_root) || location_has_mount(out, mount_point)) {
            free(mount_point); free(source); free(fs_type); continue;
        }
        if (out->count == cap) {
            size_t next = cap ? cap * 2 : 16;
            FmLocation *grown = realloc(out->items, next * sizeof(*grown));
            if (!grown) { saved = ENOMEM; free(mount_point); free(source); free(fs_type); rc = -1; break; }
            out->items = grown;
            cap = next;
        }
        FmLocation *loc = &out->items[out->count];
        memset(loc, 0, sizeof(*loc));
        loc->mount_point = mount_point;
        loc->source = source;
        loc->fs_type = fs_type;
        loc->read_only = mount_options_read_only(fields[5]);
        if (is_root || pseudo) loc->kind = FM_LOCATION_SYSTEM;
        else if (fs_type_is_network(fs_type)) loc->kind = FM_LOCATION_NETWORK;
        else loc->kind = FM_LOCATION_LOCAL;

        struct statvfs sv;
        if (statvfs(loc->mount_point, &sv) == 0) {
            uint64_t unit = (uint64_t)(sv.f_frsize ? sv.f_frsize : sv.f_bsize);
            loc->total_bytes = (uint64_t)sv.f_blocks * unit;
            loc->available_bytes = (uint64_t)sv.f_bavail * unit;
            loc->capacity_known = 1;
        }
        out->count++;
    }
    if (ferror(f) && rc == 0) { saved = errno ? errno : EIO; rc = -1; }
    free(line);
    if (fclose(f) != 0 && rc == 0) { saved = errno; rc = -1; }
    if (rc != 0) {
        errno = saved ? saved : EIO;
        set_err(err, errn, mountinfo_path);
        fm_free_locations(out);
        return -1;
    }
    qsort(out->items, out->count, sizeof(*out->items), location_cmp);
    return 0;
}

int fm_join_path(const char *base, const char *name, char *out, size_t n) {
    if (!base || !*base || !name || !out || n == 0) { errno = EINVAL; return -1; }
    size_t blen = strlen(base), nlen = strlen(name);
    int slash = blen > 0 && base[blen - 1] != '/';
    if (blen + (size_t)slash + nlen + 1 > n) { errno = ENAMETOOLONG; return -1; }
    memcpy(out, base, blen);
    size_t u = blen;
    if (slash) out[u++] = '/';
    memcpy(out + u, name, nlen);
    out[u + nlen] = '\0';
    return 0;
}

int fm_parent_path(const char *path, char *out, size_t n) {
    if (!path || !*path || !out || n < 2) { errno = EINVAL; return -1; }
    char resolved[PATH_MAX];
    if (!realpath(path, resolved)) return -1;
    if (strcmp(resolved, "/") == 0) {
        if (n < 2) { errno = ENAMETOOLONG; return -1; }
        strcpy(out, "/");
        return 0;
    }
    char *slash = strrchr(resolved, '/');
    if (!slash) { errno = EINVAL; return -1; }
    if (slash == resolved) slash[1] = '\0';
    else *slash = '\0';
    if (strlen(resolved) + 1 > n) { errno = ENAMETOOLONG; return -1; }
    strcpy(out, resolved);
    return 0;
}

int fm_load_directory(const char *path, int show_hidden, FmDirectory *out, char *err, size_t errn) {
    if (!path || !out) { errno = EINVAL; set_err(err, errn, "directory"); return -1; }
    out->entries = NULL;
    out->count = 0;
    DIR *dp = opendir(path);
    if (!dp) { set_err(err, errn, path); return -1; }
    size_t cap = 0;
    int rc = 0, saved = 0;
    for (;;) {
        errno = 0;
        struct dirent *de = readdir(dp);
        if (!de) { if (errno) { saved = errno; rc = -1; set_err(err, errn, path); } break; }
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        if (!show_hidden && de->d_name[0] == '.') continue;
        if (out->count == cap) {
            size_t next = cap ? cap * 2 : 32;
            FmEntry *grown = realloc(out->entries, next * sizeof(*grown));
            if (!grown) { saved = ENOMEM; errno = saved; set_err(err, errn, "realloc"); rc = -1; break; }
            out->entries = grown;
            cap = next;
        }
        char full[PATH_MAX];
        if (fm_join_path(path, de->d_name, full, sizeof(full)) != 0) {
            saved = errno; set_err(err, errn, de->d_name); rc = -1; break;
        }
        struct stat st;
        if (lstat(full, &st) != 0) continue;
        FmEntry *e = &out->entries[out->count];
        memset(e, 0, sizeof(*e));
        e->name = strdup(de->d_name);
        if (!e->name) { saved = ENOMEM; errno = saved; set_err(err, errn, "strdup"); rc = -1; break; }
        e->type = entry_type_from_mode(st.st_mode);
        e->mode = st.st_mode;
        e->size = st.st_size;
        e->uid = st.st_uid;
        e->gid = st.st_gid;
        e->mtime = st.st_mtime;
        out->count++;
    }
    if (closedir(dp) != 0 && rc == 0) { saved = errno; set_err(err, errn, path); rc = -1; }
    if (rc != 0) { errno = saved; fm_free_directory(out); return -1; }
    qsort(out->entries, out->count, sizeof(*out->entries), entry_cmp);
    return 0;
}

int fm_destructive_path_allowed(const char *path) {
    if (!path || !*path) return 0;
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') len--;
    if ((len == 1 && path[0] == '/') || (len == 1 && path[0] == '.') ||
        (len == 2 && path[0] == '.' && path[1] == '.')) return 0;
    int all_slash = 1;
    for (size_t i = 0; i < len; ++i) if (path[i] != '/') { all_slash = 0; break; }
    if (all_slash) return 0;
    struct stat st;
    if (lstat(path, &st) == 0 && S_ISLNK(st.st_mode)) return 1;
    char resolved[PATH_MAX];
    if (realpath(path, resolved) && strcmp(resolved, "/") == 0) return 0;
    return 1;
}

int fm_create_file(const char *path, mode_t mode, char *err, size_t errn) {
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, mode & 07777);
    if (fd < 0) { set_err(err, errn, path); return -1; }
    if (close(fd) != 0) { set_err(err, errn, path); return -1; }
    return 0;
}

int fm_create_directory(const char *path, mode_t mode, char *err, size_t errn) {
    if (mkdir(path, mode & 07777) != 0) { set_err(err, errn, path); return -1; }
    return 0;
}

static int copy_regular(const char *src, const char *dst, const struct stat *st, char *err, size_t errn) {
    int in = open(src, O_RDONLY);
    if (in < 0) { set_err(err, errn, src); return -1; }
    int out = open(dst, O_WRONLY | O_CREAT | O_EXCL, st->st_mode & 07777);
    if (out < 0) { int saved = errno; close(in); errno = saved; set_err(err, errn, dst); return -1; }
    char buf[65536];
    int rc = 0;
    for (;;) {
        ssize_t nr = read(in, buf, sizeof(buf));
        if (nr < 0) { if (errno == EINTR) continue; set_err(err, errn, src); rc = -1; break; }
        if (nr == 0) break;
        ssize_t off = 0;
        while (off < nr) {
            ssize_t nw = write(out, buf + off, (size_t)(nr - off));
            if (nw < 0) { if (errno == EINTR) continue; set_err(err, errn, dst); rc = -1; break; }
            off += nw;
        }
        if (rc != 0) break;
    }
    int saved = errno;
    if (rc == 0 && fchmod(out, st->st_mode & 07777) != 0) { saved = errno; rc = -1; set_err(err, errn, dst); }
    if (close(in) != 0 && rc == 0) { saved = errno; rc = -1; set_err(err, errn, src); }
    if (close(out) != 0 && rc == 0) { saved = errno; rc = -1; set_err(err, errn, dst); }
    errno = saved;
    if (rc != 0) unlink(dst);
    return rc;
}

static int copy_symlink(const char *src, const char *dst, char *err, size_t errn) {
    char target[PATH_MAX];
    ssize_t n = readlink(src, target, sizeof(target) - 1);
    if (n < 0) { set_err(err, errn, src); return -1; }
    target[n] = '\0';
    if (symlink(target, dst) != 0) { set_err(err, errn, dst); return -1; }
    return 0;
}

static int path_is_same_or_descendant(const char *src_dir, const char *dst) {
    char src_real[PATH_MAX], dst_parent[PATH_MAX], parent_real[PATH_MAX];
    if (!realpath(src_dir, src_real)) return 0;
    if (strlen(dst) + 1 > sizeof(dst_parent)) return 1;
    strcpy(dst_parent, dst);
    char *slash = strrchr(dst_parent, '/');
    const char *parent = ".";
    if (slash) {
        if (slash == dst_parent) slash[1] = '\0';
        else *slash = '\0';
        parent = dst_parent;
    }
    if (!realpath(parent, parent_real)) return 0;
    size_t n = strlen(src_real);
    return strcmp(parent_real, src_real) == 0 ||
           (strncmp(parent_real, src_real, n) == 0 && parent_real[n] == '/');
}

static int copy_path_internal(const char *src, const char *dst, char *err, size_t errn) {
    struct stat st;
    if (lstat(src, &st) != 0) { set_err(err, errn, src); return -1; }
    if (S_ISREG(st.st_mode)) return copy_regular(src, dst, &st, err, errn);
    if (S_ISLNK(st.st_mode)) return copy_symlink(src, dst, err, errn);
    if (!S_ISDIR(st.st_mode)) { errno = ENOTSUP; set_err(err, errn, src); return -1; }

    if (mkdir(dst, 0700) != 0) { set_err(err, errn, dst); return -1; }
    DIR *dp = opendir(src);
    if (!dp) { int saved = errno; rmdir(dst); errno = saved; set_err(err, errn, src); return -1; }
    int rc = 0, saved = 0;
    for (;;) {
        errno = 0;
        struct dirent *de = readdir(dp);
        if (!de) { if (errno) { saved = errno; set_err(err, errn, src); rc = -1; } break; }
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        char sp[PATH_MAX], dp_path[PATH_MAX];
        if (fm_join_path(src, de->d_name, sp, sizeof(sp)) != 0 ||
            fm_join_path(dst, de->d_name, dp_path, sizeof(dp_path)) != 0) {
            saved = errno; set_err(err, errn, de->d_name); rc = -1; break;
        }
        if (copy_path_internal(sp, dp_path, err, errn) != 0) { saved = errno; rc = -1; break; }
    }
    if (closedir(dp) != 0 && rc == 0) { saved = errno; set_err(err, errn, src); rc = -1; }
    if (rc == 0 && chmod(dst, st.st_mode & 07777) != 0) { saved = errno; set_err(err, errn, dst); rc = -1; }
    if (rc != 0) errno = saved;
    return rc;
}

static int require_destination_absent(const char *dst, char *err, size_t errn) {
    struct stat st;
    if (lstat(dst, &st) == 0) { errno = EEXIST; set_err(err, errn, dst); return -1; }
    if (errno != ENOENT) { set_err(err, errn, dst); return -1; }
    return 0;
}

int fm_copy_path(const char *src, const char *dst, char *err, size_t errn) {
    if (!src || !dst || !*src || !*dst) { errno = EINVAL; set_err(err, errn, "copy"); return -1; }
    struct stat st;
    if (lstat(src, &st) != 0) { set_err(err, errn, src); return -1; }
    if (require_destination_absent(dst, err, errn) != 0) return -1;
    if (S_ISDIR(st.st_mode) && path_is_same_or_descendant(src, dst)) {
        errno = EINVAL;
        set_text_err(err, errn, "destination is inside source directory");
        return -1;
    }
    if (copy_path_internal(src, dst, err, errn) != 0) {
        int saved = errno;
        fm_remove_path(dst, 1, NULL, 0);
        errno = saved;
        return -1;
    }
    return 0;
}

int fm_remove_path(const char *path, int recursive, char *err, size_t errn) {
    if (!fm_destructive_path_allowed(path)) { errno = EPERM; set_text_err(err, errn, "unsafe destructive path rejected"); return -1; }
    struct stat st;
    if (lstat(path, &st) != 0) { set_err(err, errn, path); return -1; }
    if (!S_ISDIR(st.st_mode)) {
        if (unlink(path) != 0) { set_err(err, errn, path); return -1; }
        return 0;
    }
    if (!recursive) {
        if (rmdir(path) != 0) { set_err(err, errn, path); return -1; }
        return 0;
    }
    DIR *dp = opendir(path);
    if (!dp) { set_err(err, errn, path); return -1; }
    int rc = 0, saved = 0;
    for (;;) {
        errno = 0;
        struct dirent *de = readdir(dp);
        if (!de) { if (errno) { saved = errno; set_err(err, errn, path); rc = -1; } break; }
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        char child[PATH_MAX];
        if (fm_join_path(path, de->d_name, child, sizeof(child)) != 0) { saved = errno; set_err(err, errn, de->d_name); rc = -1; break; }
        if (fm_remove_path(child, 1, err, errn) != 0) { saved = errno; rc = -1; break; }
    }
    if (closedir(dp) != 0 && rc == 0) { saved = errno; set_err(err, errn, path); rc = -1; }
    if (rc == 0 && rmdir(path) != 0) { saved = errno; set_err(err, errn, path); rc = -1; }
    if (rc != 0) errno = saved;
    return rc;
}

int fm_move_path(const char *src, const char *dst, char *err, size_t errn) {
    if (!src || !dst || !*src || !*dst) { errno = EINVAL; set_err(err, errn, "move"); return -1; }
    if (require_destination_absent(dst, err, errn) != 0) return -1;
    if (rename(src, dst) == 0) return 0;
    if (errno != EXDEV) { set_err(err, errn, src); return -1; }
    if (fm_copy_path(src, dst, err, errn) != 0) return -1;
    if (fm_remove_path(src, 1, err, errn) != 0) {
        int saved = errno;
        fm_remove_path(dst, 1, NULL, 0);
        errno = saved;
        return -1;
    }
    return 0;
}

int fm_parse_mode(const char *text, mode_t *out) {
    if (!text || !out) return -1;
    size_t n = strlen(text);
    if (n != 3 && n != 4) return -1;
    unsigned value = 0;
    for (size_t i = 0; i < n; ++i) {
        if (text[i] < '0' || text[i] > '7') return -1;
        value = (value << 3) | (unsigned)(text[i] - '0');
    }
    if (value > 07777) return -1;
    *out = (mode_t)value;
    return 0;
}

int fm_chmod_path(const char *path, mode_t mode, char *err, size_t errn) {
    if (chmod(path, mode & 07777) != 0) { set_err(err, errn, path); return -1; }
    return 0;
}

const char *fm_type_name_en(FmEntryType type) {
    switch (type) {
        case FM_ENTRY_DIRECTORY: return "Directory";
        case FM_ENTRY_REGULAR: return "File";
        case FM_ENTRY_SYMLINK: return "Symlink";
        default: return "Other";
    }
}
