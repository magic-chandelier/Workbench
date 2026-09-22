#ifndef WB_PLUGINS_H
#define WB_PLUGINS_H

#include <stddef.h>
#include <sys/types.h>
#include "include/version.h"

#define WB_MAX_PLUGINS 256
#define WB_PLUGIN_PATH_MAX 4096
#define WB_PLUGIN_API_MIN_SUPPORTED 1

typedef enum { WB_PLUGIN_SYSTEM=0, WB_PLUGIN_USER=1 } WbPluginSource;
typedef enum { WB_PLUGIN_ACTIVE=0, WB_PLUGIN_INVALID, WB_PLUGIN_INCOMPATIBLE, WB_PLUGIN_SHADOWED, WB_PLUGIN_DISABLED } WbPluginStatus;

typedef struct {
    char id[64];
    char version[64];
    int format;
    int api_min;
    int api_max;
    char name_zh[128];
    char name_en[128];
    char description_zh[256];
    char description_en[256];
    char root[WB_PLUGIN_PATH_MAX];
    char entry[512];
    char command_sets[512];
    char file_actions[512];
    char preview_providers[512];
    WbPluginSource source;
    WbPluginStatus status;
    char error[192];
    int enabled;
} WbPlugin;

typedef struct {
    int exited;
    int exit_code;
    int signaled;
    int term_signal;
} WbPluginRunResult;

typedef struct {
    WbPlugin items[WB_MAX_PLUGINS];
    size_t count;
    char system_root[WB_PLUGIN_PATH_MAX];
    char user_root[WB_PLUGIN_PATH_MAX];
} WbPluginRegistry;

int wb_plugins_valid_id(const char *id);
int wb_plugin_api_compatible(int api_min,int api_max);
int wb_plugin_supports_api(const WbPlugin *plugin,int api_version);
int wb_plugins_default_roots(char *system_root,size_t system_cap,char *user_root,size_t user_cap);
void wb_plugin_registry_init(WbPluginRegistry *registry);
void wb_plugin_registry_clear(WbPluginRegistry *registry);
int wb_plugin_registry_scan(WbPluginRegistry *registry,const char *system_root,const char *user_root);
size_t wb_plugin_registry_active_count(const WbPluginRegistry *registry);
const WbPlugin *wb_plugin_registry_active_at(const WbPluginRegistry *registry,size_t index);
const WbPlugin *wb_plugin_registry_find_active(const WbPluginRegistry *registry,const char *id);
int wb_plugin_launch(const WbPlugin *plugin,WbPluginRunResult *result,char *err,size_t err_cap);
int wb_plugin_launch_file_action(const WbPlugin *plugin,const char *entry,const char *action_id,const char *selected_path,WbPluginRunResult *result,char *err,size_t err_cap);
int wb_plugin_preview_list(const WbPlugin *plugin,const char *entry,const char *provider_id,const char *archive,char **output,size_t *output_len,char *err,size_t err_cap);
int wb_plugin_preview_materialize(const WbPlugin *plugin,const char *entry,const char *provider_id,const char *archive,const char *member,const char *destination,WbPluginRunResult *result,char *err,size_t err_cap);
int wb_plugin_uninstall(const WbPluginRegistry *registry,const WbPlugin *plugin,char *err,size_t err_cap);
int wb_plugin_remove_system_by_id(const char *system_root,const char *id,char *err,size_t err_cap);
int wb_plugin_set_enabled(const WbPluginRegistry *registry,const WbPlugin *plugin,int enabled,char *err,size_t err_cap);

#endif
