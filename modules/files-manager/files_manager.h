#ifndef WORKBENCH_FILES_MANAGER_H
#define WORKBENCH_FILES_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

typedef enum {
    FM_ENTRY_REGULAR = 0,
    FM_ENTRY_DIRECTORY,
    FM_ENTRY_SYMLINK,
    FM_ENTRY_OTHER
} FmEntryType;

typedef struct {
    char *name;
    FmEntryType type;
    mode_t mode;
    off_t size;
    uid_t uid;
    gid_t gid;
    time_t mtime;
} FmEntry;

typedef struct {
    FmEntry *entries;
    size_t count;
} FmDirectory;

typedef enum {
    FM_LOCATION_SYSTEM = 0,
    FM_LOCATION_LOCAL,
    FM_LOCATION_NETWORK
} FmLocationKind;

typedef struct {
    char *mount_point;
    char *source;
    char *fs_type;
    FmLocationKind kind;
    uint64_t total_bytes;
    uint64_t available_bytes;
    int capacity_known;
    int read_only;
} FmLocation;

typedef struct {
    FmLocation *items;
    size_t count;
} FmLocations;

int fm_load_directory(const char *path, int show_hidden, FmDirectory *out, char *err, size_t errn);
void fm_free_directory(FmDirectory *dir);

int fm_load_locations(const char *mountinfo_path, int show_system, FmLocations *out, char *err, size_t errn);
void fm_free_locations(FmLocations *locations);

int fm_join_path(const char *base, const char *name, char *out, size_t n);
int fm_parent_path(const char *path, char *out, size_t n);
int fm_destructive_path_allowed(const char *path);

int fm_create_file(const char *path, mode_t mode, char *err, size_t errn);
int fm_create_directory(const char *path, mode_t mode, char *err, size_t errn);
int fm_copy_path(const char *src, const char *dst, char *err, size_t errn);
int fm_move_path(const char *src, const char *dst, char *err, size_t errn);
int fm_remove_path(const char *path, int recursive, char *err, size_t errn);
int fm_parse_mode(const char *text, mode_t *out);
int fm_chmod_path(const char *path, mode_t mode, char *err, size_t errn);

const char *fm_type_name_en(FmEntryType type);

#endif
