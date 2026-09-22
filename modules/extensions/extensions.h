#ifndef WB_EXTENSIONS_H
#define WB_EXTENSIONS_H

#include <stddef.h>
#include <sys/types.h>
#include "modules/plugins/plugins.h"

#define WB_MAX_EXTENSION_ACTIONS 256
#define WB_MAX_PREVIEW_PROVIDERS 128

typedef enum {
    WB_FILE_ACTION_ANY = 0,
    WB_FILE_ACTION_FILE,
    WB_FILE_ACTION_DIRECTORY
} WbFileActionTarget;

typedef struct {
    const char *id;
    const char *title_zh;
    const char *title_en;
    const char *source;
    const char *entry;
    const char *extensions;
    WbFileActionTarget target;
} WbExtensionActionDescriptor;

typedef struct {
    char id[64];
    char title_zh[128];
    char title_en[128];
    char source[64];
    char entry[512];
    char extensions[512];
    char descriptor[512];
    WbFileActionTarget target;
} WbRegisteredExtensionAction;

typedef struct {
    char id[64];
    char title_zh[128];
    char title_en[128];
    char source[64];
    char entry[512];
    char extensions[512];
    char descriptor[512];
} WbRegisteredPreviewProvider;

typedef struct {
    WbRegisteredExtensionAction actions[WB_MAX_EXTENSION_ACTIONS];
    size_t count;
    WbRegisteredPreviewProvider previews[WB_MAX_PREVIEW_PROVIDERS];
    size_t preview_count;
} WbExtensionRegistry;

void wb_extension_registry_init(WbExtensionRegistry *registry);
size_t wb_extension_registry_count(const WbExtensionRegistry *registry);
int wb_extension_register_action(WbExtensionRegistry *registry,const WbExtensionActionDescriptor *descriptor);
const WbRegisteredExtensionAction *wb_extension_action_at(const WbExtensionRegistry *registry,size_t index);
const WbRegisteredExtensionAction *wb_extension_find_action(const WbExtensionRegistry *registry,const char *source,const char *id);
int wb_extension_registry_load_plugin_actions(WbExtensionRegistry *registry,const WbPluginRegistry *plugins);
int wb_extension_action_matches_path(const WbRegisteredExtensionAction *action,const char *path,mode_t mode);
int wb_extension_action_revalidate(const WbPlugin *plugin,const WbRegisteredExtensionAction *action,char *err,size_t err_cap);
size_t wb_extension_preview_count(const WbExtensionRegistry *registry);
const WbRegisteredPreviewProvider *wb_extension_preview_at(const WbExtensionRegistry *registry,size_t index);
const WbRegisteredPreviewProvider *wb_extension_find_preview(const WbExtensionRegistry *registry,const char *source,const char *id);
int wb_extension_preview_matches_path(const WbRegisteredPreviewProvider *provider,const char *path,mode_t mode);
int wb_extension_preview_revalidate(const WbPlugin *plugin,const WbRegisteredPreviewProvider *provider,char *err,size_t err_cap);

#endif
