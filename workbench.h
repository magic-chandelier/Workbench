#ifndef WORKBENCH_H
#define WORKBENCH_H

#include <stddef.h>

typedef enum { LANG_ZH = 0, LANG_EN = 1 } Language;
typedef enum { PROFILE_GENERIC = 0, PROFILE_CENTOS } LinuxProfile;
typedef enum {
    CAT_ALL = 0, CAT_SYSTEM, CAT_FILES, CAT_TEXT, CAT_PROCESS,
    CAT_NETWORK, CAT_STORAGE, CAT_PERMISSION, CAT_ARCHIVE, CAT_USER,
    CAT_PACKAGE, CAT_SERVICE, CAT_FIREWALL, CAT_SELINUX, CAT_COUNT
} Category;
typedef enum { RISK_NORMAL = 0, RISK_SENSITIVE, RISK_PRIVILEGED } Risk;
typedef enum { ARG_NONE = 0, ARG_TEXT, ARG_PATH, ARG_DESTRUCTIVE_PATH, ARG_UINT, ARG_PORT, ARG_PID } ArgKind;

#define WB_MAX_ARGS 4

typedef struct {
    const char *prompt_zh;
    const char *prompt_en;
    const char *default_value;
    ArgKind kind;
} TaskArg;

typedef struct {
    const char *id;
    const char *title_zh, *title_en;
    const char *desc_zh, *desc_en;
    const char *keywords;
    Category category;
    Risk risk;
    const char *command;
    const char *source_id;
    TaskArg args[WB_MAX_ARGS];
} Task;

typedef struct {
    const char *id;
    const char *title_zh;
    const char *title_en;
    const char *desc_zh;
    const char *desc_en;
    const Task *tasks;
    size_t task_count;
} Module;

#endif
