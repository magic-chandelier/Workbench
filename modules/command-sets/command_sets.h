#ifndef WB_COMMAND_SETS_H
#define WB_COMMAND_SETS_H

#include <stddef.h>
#include "workbench.h"

#define WB_MAX_USER_COMMAND_SETS 64
#define WB_MAX_USER_ACTIONS 256

typedef enum {
    COMMAND_SET_SYSTEM = 0,
    COMMAND_SET_USER = 1,
    COMMAND_SET_PLUGIN = 2
} CommandSetSource;

typedef struct WbUserTask WbUserTask;

typedef struct {
    char id[64];
    char name_zh[128];
    char name_en[128];
    char desc_zh[256];
    char desc_en[256];
    CommandSetSource source;
    int read_only;
    int deleted;
    const Task *system_tasks;
    size_t system_task_count;
    WbUserTask *user_tasks;
    size_t user_task_count;
    size_t user_task_capacity;
    char path[4096];
} CommandSet;

typedef struct {
    CommandSet sets[WB_MAX_USER_COMMAND_SETS + 1];
    size_t count;
    char root[4096];
} CommandSetStore;

int wb_command_set_init_system_linux(CommandSet *set,const Task *tasks,size_t task_count);
size_t wb_command_set_task_count(const CommandSet *set);
const Task *wb_command_set_task_at(const CommandSet *set,size_t index);
int wb_command_set_can_edit(const CommandSet *set);
int wb_command_set_rename(CommandSet *set,const char *name_zh,const char *name_en);
int wb_command_set_mark_deleted(CommandSet *set);
void wb_command_set_destroy(CommandSet *set);

int wb_command_sets_init(CommandSetStore *store,const Task *system_tasks,size_t system_task_count,const char *root);
int wb_command_sets_default_root(char *out,size_t cap);
int wb_command_sets_load_user(CommandSetStore *store);
CommandSet *wb_command_sets_find(CommandSetStore *store,const char *id);
const CommandSet *wb_command_sets_find_const(const CommandSetStore *store,const char *id);
int wb_command_set_create_user(CommandSetStore *store,const char *id,const char *name_zh,const char *name_en,const char *description,CommandSet **out);
int wb_command_set_save(CommandSetStore *store,CommandSet *set);
int wb_command_set_update_metadata(CommandSetStore *store,CommandSet *set,const char *name_zh,const char *name_en,const char *desc_zh,const char *desc_en);
int wb_command_set_delete_user(CommandSetStore *store,CommandSet *set);
void wb_command_sets_free(CommandSetStore *store);

const Task *wb_command_action_find(const CommandSet *set,const char *id);
int wb_command_action_add(CommandSetStore *store,CommandSet *set,const Task *task,size_t *out_index);
int wb_command_action_update(CommandSetStore *store,CommandSet *set,const char *existing_id,const Task *task);
int wb_command_action_delete(CommandSetStore *store,CommandSet *set,const char *id);

#endif
