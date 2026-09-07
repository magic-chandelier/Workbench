#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "modules/files-manager/files_manager.h"

#define CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); return 1; } } while (0)

static int write_text(const char *path, const char *text) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    size_t n = strlen(text), off = 0;
    while (off < n) {
        ssize_t w = write(fd, text + off, n - off);
        if (w < 0) { if (errno == EINTR) continue; close(fd); return -1; }
        off += (size_t)w;
    }
    return close(fd);
}

static int read_text(const char *path, char *buf, size_t cap) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, cap - 1);
    int saved = errno;
    close(fd);
    errno = saved;
    if (n < 0) return -1;
    buf[n] = '\0';
    return 0;
}

static int has_name(const FmDirectory *dir, const char *name) {
    for (size_t i = 0; i < dir->count; ++i) if (strcmp(dir->entries[i].name, name) == 0) return 1;
    return 0;
}

static const FmLocation *find_location(const FmLocations *locations, const char *mount_point) {
    for (size_t i = 0; i < locations->count; ++i)
        if (strcmp(locations->items[i].mount_point, mount_point) == 0) return &locations->items[i];
    return NULL;
}

int main(void) {
    char root_template[] = "/tmp/wb-fm-test-XXXXXX";
    char *root = mkdtemp(root_template);
    CHECK(root != NULL, "mkdtemp");

    char p[4096], q[4096], r[4096], err[256];
    CHECK(fm_join_path(root, "visible.txt", p, sizeof(p)) == 0, "join visible");
    CHECK(write_text(p, "hello\n") == 0, "write visible");
    CHECK(fm_join_path(root, ".hidden", q, sizeof(q)) == 0, "join hidden");
    CHECK(write_text(q, "secret\n") == 0, "write hidden");
    CHECK(fm_join_path(root, "sub", q, sizeof(q)) == 0, "join sub");
    CHECK(mkdir(q, 0755) == 0, "mkdir sub");
    CHECK(fm_join_path(root, "link", q, sizeof(q)) == 0, "join link");
    CHECK(symlink("visible.txt", q) == 0, "create symlink");

    FmDirectory dir = {0};
    CHECK(fm_load_directory(root, 0, &dir, err, sizeof(err)) == 0, err);
    CHECK(has_name(&dir, "visible.txt"), "visible entry listed");
    CHECK(has_name(&dir, "sub"), "directory listed");
    CHECK(has_name(&dir, "link"), "symlink listed");
    CHECK(!has_name(&dir, ".hidden"), "hidden entry filtered");
    CHECK(dir.count >= 1 && dir.entries[0].type == FM_ENTRY_DIRECTORY, "directories sort first");
    fm_free_directory(&dir);

    CHECK(fm_load_directory(root, 1, &dir, err, sizeof(err)) == 0, err);
    CHECK(has_name(&dir, ".hidden"), "hidden entry shown");
    fm_free_directory(&dir);

    /* Linux mount discovery: user storage is visible, pseudo/system mounts are hidden by default. */
    char data_dir[4096], nas_dir[4096], run_dir[4096], spaced_dir[4096], file_mount[4096], mountinfo[4096];
    CHECK(fm_join_path(root, "data", data_dir, sizeof(data_dir)) == 0 && mkdir(data_dir, 0755) == 0, "mkdir data mount fixture");
    CHECK(fm_join_path(root, "nas", nas_dir, sizeof(nas_dir)) == 0 && mkdir(nas_dir, 0755) == 0, "mkdir nas mount fixture");
    CHECK(fm_join_path(root, "run", run_dir, sizeof(run_dir)) == 0 && mkdir(run_dir, 0755) == 0, "mkdir tmpfs fixture");
    CHECK(fm_join_path(root, "space mount", spaced_dir, sizeof(spaced_dir)) == 0 && mkdir(spaced_dir, 0755) == 0, "mkdir escaped mount fixture");
    CHECK(fm_join_path(root, "bound-file", file_mount, sizeof(file_mount)) == 0 && write_text(file_mount, "bind\n") == 0, "create file mount fixture");
    CHECK(fm_join_path(root, "mountinfo", mountinfo, sizeof(mountinfo)) == 0, "join mountinfo fixture");
    char escaped_spaced[8192]; size_t eu = 0;
    for (const char *sp = spaced_dir; *sp && eu + 5 < sizeof(escaped_spaced); ++sp) {
        if (*sp == ' ') { memcpy(escaped_spaced + eu, "\\040", 4); eu += 4; }
        else escaped_spaced[eu++] = *sp;
    }
    escaped_spaced[eu] = '\0';
    FILE *mf = fopen(mountinfo, "w");
    CHECK(mf != NULL, "open mountinfo fixture");
    fprintf(mf, "24 1 0:1 / / rw,relatime - overlay overlay rw\n");
    fprintf(mf, "25 24 8:17 / %s rw,relatime - xfs /dev/sdb1 rw\n", data_dir);
    fprintf(mf, "26 24 0:42 / %s rw,relatime - nfs4 server:/share rw\n", nas_dir);
    fprintf(mf, "27 24 0:43 / %s rw,nosuid,nodev - tmpfs tmpfs rw\n", run_dir);
    fprintf(mf, "28 24 8:18 / %s ro,relatime - ext4 /dev/sdc1 ro\n", escaped_spaced);
    fprintf(mf, "29 24 8:19 / %s rw,relatime - xfs /dev/duplicate rw\n", data_dir);
    fprintf(mf, "30 24 8:20 / %s rw,relatime - xfs /dev/file-bind rw\n", file_mount);
    CHECK(fclose(mf) == 0, "close mountinfo fixture");

    FmLocations locations = {0};
    CHECK(fm_load_locations(mountinfo, 0, &locations, err, sizeof(err)) == 0, err);
    const FmLocation *root_loc = find_location(&locations, "/");
    const FmLocation *data_loc = find_location(&locations, data_dir);
    const FmLocation *nas_loc = find_location(&locations, nas_dir);
    const FmLocation *space_loc = find_location(&locations, spaced_dir);
    CHECK(root_loc && root_loc->kind == FM_LOCATION_SYSTEM, "root mount retained as system location");
    CHECK(data_loc && data_loc->kind == FM_LOCATION_LOCAL, "local block mount classified");
    CHECK(nas_loc && nas_loc->kind == FM_LOCATION_NETWORK, "network mount classified");
    CHECK(space_loc && strcmp(space_loc->source, "/dev/sdc1") == 0, "mountinfo escaped path decoded");
    CHECK(space_loc->read_only == 1, "read-only mount detected");
    CHECK(find_location(&locations, run_dir) == NULL, "tmpfs hidden by default");
    CHECK(find_location(&locations, file_mount) == NULL, "file mount omitted from navigable locations");
    CHECK(data_loc->capacity_known && data_loc->total_bytes > 0, "capacity collected with statvfs");
    size_t data_rows = 0;
    for (size_t i = 0; i < locations.count; ++i) if (strcmp(locations.items[i].mount_point, data_dir) == 0) data_rows++;
    CHECK(data_rows == 1, "duplicate mount points collapsed");
    fm_free_locations(&locations);

    CHECK(fm_load_locations(mountinfo, 1, &locations, err, sizeof(err)) == 0, err);
    CHECK(find_location(&locations, run_dir) != NULL, "system tmpfs visible when requested");
    fm_free_locations(&locations);

    CHECK(fm_join_path(root, "created.txt", p, sizeof(p)) == 0, "join create file");
    CHECK(fm_create_file(p, 0644, err, sizeof(err)) == 0, err);
    struct stat st;
    CHECK(lstat(p, &st) == 0 && S_ISREG(st.st_mode), "created regular file");
    CHECK(fm_join_path(root, "created-dir", q, sizeof(q)) == 0, "join create dir");
    CHECK(fm_create_directory(q, 0755, err, sizeof(err)) == 0, err);
    CHECK(lstat(q, &st) == 0 && S_ISDIR(st.st_mode), "created directory");

    CHECK(fm_join_path(root, "copy.txt", q, sizeof(q)) == 0, "join copy file");
    CHECK(fm_join_path(root, "visible.txt", p, sizeof(p)) == 0, "join copy source");
    CHECK(fm_copy_path(p, q, err, sizeof(err)) == 0, err);
    char buf[64];
    CHECK(read_text(q, buf, sizeof(buf)) == 0 && strcmp(buf, "hello\n") == 0, "file copy content");

    CHECK(fm_join_path(root, "tree", p, sizeof(p)) == 0, "join tree");
    CHECK(mkdir(p, 0750) == 0, "mkdir tree");
    CHECK(fm_join_path(p, "nested.txt", q, sizeof(q)) == 0, "join nested");
    CHECK(write_text(q, "nested\n") == 0, "write nested");
    CHECK(fm_join_path(p, "nested-link", q, sizeof(q)) == 0, "join nested symlink");
    CHECK(symlink("nested.txt", q) == 0, "nested symlink");
    CHECK(fm_join_path(root, "tree-copy", q, sizeof(q)) == 0, "join tree copy");
    mode_t old_umask = umask(0077);
    CHECK(fm_copy_path(p, q, err, sizeof(err)) == 0, err);
    umask(old_umask);
    CHECK(lstat(q, &st) == 0 && (st.st_mode & 07777) == 0750, "directory copy preserves mode despite umask");
    CHECK(fm_join_path(q, "nested.txt", r, sizeof(r)) == 0, "join copied nested");
    CHECK(read_text(r, buf, sizeof(buf)) == 0 && strcmp(buf, "nested\n") == 0, "recursive copy content");
    CHECK(lstat(r, &st) == 0 && (st.st_mode & 07777) == 0644, "file copy preserves mode despite umask");
    CHECK(fm_join_path(q, "nested-link", r, sizeof(r)) == 0, "join copied link");
    CHECK(lstat(r, &st) == 0 && S_ISLNK(st.st_mode), "recursive copy preserves symlink");

    CHECK(fm_join_path(p, "inside-copy", q, sizeof(q)) == 0, "join descendant copy");
    CHECK(fm_copy_path(p, q, err, sizeof(err)) != 0, "reject copying directory into descendant");

    CHECK(fm_join_path(root, "move-source.txt", p, sizeof(p)) == 0, "join move src");
    CHECK(write_text(p, "move\n") == 0, "write move src");
    CHECK(fm_join_path(root, "move-dest.txt", q, sizeof(q)) == 0, "join move dst");
    CHECK(fm_move_path(p, q, err, sizeof(err)) == 0, err);
    CHECK(lstat(p, &st) != 0 && errno == ENOENT, "move source gone");
    CHECK(read_text(q, buf, sizeof(buf)) == 0 && strcmp(buf, "move\n") == 0, "move destination content");

    mode_t mode = 0;
    CHECK(fm_parse_mode("640", &mode) == 0 && mode == 0640, "parse 3-digit mode");
    CHECK(fm_parse_mode("0750", &mode) == 0 && mode == 0750, "parse 4-digit mode");
    CHECK(fm_parse_mode("0888", &mode) != 0, "reject invalid octal");
    CHECK(fm_parse_mode("12", &mode) != 0, "reject short mode");
    CHECK(fm_join_path(root, "visible.txt", p, sizeof(p)) == 0, "join chmod target");
    CHECK(fm_chmod_path(p, 0640, err, sizeof(err)) == 0, err);
    CHECK(lstat(p, &st) == 0 && (st.st_mode & 07777) == 0640, "chmod applied");

    CHECK(!fm_destructive_path_allowed("/"), "root destructive path rejected");
    CHECK(!fm_destructive_path_allowed("."), "dot destructive path rejected");
    CHECK(!fm_destructive_path_allowed(".."), "dotdot destructive path rejected");
    CHECK(fm_destructive_path_allowed(root), "specific temp path accepted");
    CHECK(fm_join_path(root, "root-link", p, sizeof(p)) == 0, "join root symlink");
    CHECK(symlink("/", p) == 0, "create root symlink");
    CHECK(fm_destructive_path_allowed(p), "symlink to root is safe to unlink");
    CHECK(fm_remove_path(p, 0, err, sizeof(err)) == 0, "unlink symlink to root");
    CHECK(lstat("/", &st) == 0 && S_ISDIR(st.st_mode), "root remains after unlinking symlink");

    CHECK(fm_join_path(root, "tree-copy", p, sizeof(p)) == 0, "join remove tree");
    CHECK(fm_remove_path(p, 1, err, sizeof(err)) == 0, err);
    CHECK(lstat(p, &st) != 0 && errno == ENOENT, "recursive delete removed tree");
    CHECK(fm_remove_path("/", 1, err, sizeof(err)) != 0, "recursive delete root rejected");

    CHECK(fm_remove_path(root, 1, err, sizeof(err)) == 0, err);
    printf("FILES MANAGER OPS PASS\n");
    return 0;
}
