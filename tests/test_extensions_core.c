#include "modules/extensions/extensions.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int main(void){
    WbExtensionRegistry r;wb_extension_registry_init(&r);assert(wb_extension_registry_count(&r)==0);
    WbExtensionActionDescriptor a={"extract-here","解压到这里","Extract here","plugin.archive","bin/archive","7z,zip,tar.gz",WB_FILE_ACTION_FILE};
    assert(wb_extension_register_action(&r,&a)==0);assert(wb_extension_registry_count(&r)==1);
    const WbRegisteredExtensionAction *x=wb_extension_action_at(&r,0);
    assert(x&&!strcmp(x->id,"extract-here")&&!strcmp(x->source,"plugin.archive")&&!strcmp(x->entry,"bin/archive"));
    assert(wb_extension_find_action(&r,"plugin.archive","extract-here")==x);
    assert(wb_extension_action_matches_path(x,"/tmp/Test.ZIP",0100000|0644));
    assert(wb_extension_action_matches_path(x,"/tmp/a.tar.gz",0100000|0644));
    assert(!wb_extension_action_matches_path(x,"/tmp/a.txt",0100000|0644));
    assert(!wb_extension_action_matches_path(x,"/tmp/folder",0040000|0755));
    assert(wb_extension_register_action(&r,&a)!=0);
    WbExtensionActionDescriptor bad={"../bad","坏","Bad","plugin.archive","bin/archive","*",WB_FILE_ACTION_ANY};
    assert(wb_extension_register_action(&r,&bad)!=0);
    puts("EXTENSIONS CORE PASS");return 0;
}
