#include <stdio.h>
#include <string.h>
#include "modules/command-sets/command_sets.h"

#define CHECK(x,msg) do { if(!(x)){ fprintf(stderr,"FAIL: %s\n",msg); return 1; } } while(0)

int main(void){
    Task tasks[2] = {
        {.id="one", .title_zh="一", .title_en="One", .desc_zh="", .desc_en="", .keywords="one", .category=CAT_SYSTEM, .risk=RISK_NORMAL, .command="true", .source_id="linux-core"},
        {.id="two", .title_zh="二", .title_en="Two", .desc_zh="", .desc_en="", .keywords="two", .category=CAT_FILES, .risk=RISK_NORMAL, .command="true", .source_id="linux-core"}
    };
    CommandSet set;
    CHECK(wb_command_set_init_system_linux(&set,tasks,2)==0,"init system set");
    CHECK(set.source==COMMAND_SET_SYSTEM,"system source");
    CHECK(set.read_only==1,"system read-only");
    CHECK(strcmp(set.id,"linux")==0,"linux id");
    CHECK(wb_command_set_task_count(&set)==2,"system task count");
    CHECK(wb_command_set_task_at(&set,1)==&tasks[1],"system task passthrough");
    CHECK(!wb_command_set_can_edit(&set),"cannot edit system set");
    CHECK(wb_command_set_rename(&set,"Renamed","Renamed")!=0,"system rename rejected");
    CHECK(wb_command_set_mark_deleted(&set)!=0,"system delete rejected");
    wb_command_set_destroy(&set);
    puts("COMMAND SETS CORE PASS");
    return 0;
}
