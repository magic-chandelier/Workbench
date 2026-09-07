#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "modules/command-sets/command_sets.h"

#define CHECK(x,msg) do { if(!(x)){ fprintf(stderr,"FAIL: %s\n",msg); return 1; } } while(0)

static void rm_tree(const char *root){char cmd[8192];snprintf(cmd,sizeof(cmd),"rm -rf -- '%s'",root);system(cmd);}

int main(void){
    char tmp[]="/tmp/wb-command-actions-XXXXXX";CHECK(mkdtemp(tmp)!=NULL,"mkdtemp");
    char root[4096];snprintf(root,sizeof(root),"%s/sets",tmp);
    Task sys={.id="system.one",.title_zh="系统",.title_en="System",.desc_zh="",.desc_en="",.keywords="",.category=CAT_SYSTEM,.risk=RISK_NORMAL,.command="true",.source_id="linux-core"};
    CommandSetStore store;CHECK(wb_command_sets_init(&store,&sys,1,root)==0,"init");
    CommandSet *user=NULL;CHECK(wb_command_set_create_user(&store,"mine","我的","Mine","custom",&user)==0,"create user");

    Task simple={.id="hello",.title_zh="你好",.title_en="Hello",.desc_zh="输出",.desc_en="Output",.keywords="hello custom",.category=CAT_ALL,.risk=RISK_NORMAL,.command="printf 'hello\\n'",.source_id="user"};
    size_t idx=999;CHECK(wb_command_action_add(&store,user,&simple,&idx)==0&&idx==0,"add simple");
    CHECK(wb_command_set_task_count(user)==1,"count one");
    const Task *got=wb_command_action_find(user,"hello");CHECK(got&&strcmp(got->command,simple.command)==0,"find simple");

    Task templ={.id="connect",.title_zh="连接",.title_en="Connect",.desc_zh="SSH",.desc_en="SSH",.keywords="ssh host",.category=CAT_NETWORK,.risk=RISK_SENSITIVE,.command="ssh {1}@{2}",.source_id="user",
        .args={{"用户名","User","root",ARG_TEXT},{"主机","Host","127.0.0.1",ARG_TEXT},{0},{0}}};
    CHECK(wb_command_action_add(&store,user,&templ,NULL)==0,"add templated");
    CHECK(wb_command_action_add(&store,user,&templ,NULL)!=0,"duplicate action id rejected");

    Task edited=templ;edited.title_zh="SSH 到主机";edited.title_en="SSH to host";edited.command="ssh -p {3} {1}@{2}";edited.risk=RISK_PRIVILEGED;
    edited.args[2]=(TaskArg){"端口","Port","22",ARG_PORT};
    CHECK(wb_command_action_update(&store,user,"connect",&edited)==0,"update action");

    CommandSetStore loaded;CHECK(wb_command_sets_init(&loaded,&sys,1,root)==0,"reload init");CHECK(wb_command_sets_load_user(&loaded)==0,"reload user");
    CommandSet *re=wb_command_sets_find(&loaded,"mine");CHECK(re!=NULL,"find set reloaded");
    CHECK(wb_command_set_task_count(re)==2,"two actions reloaded");
    got=wb_command_action_find(re,"connect");CHECK(got!=NULL,"find connect reloaded");
    CHECK(strcmp(got->title_zh,"SSH 到主机")==0,"edited title roundtrip");
    CHECK(strcmp(got->command,"ssh -p {3} {1}@{2}")==0,"edited command roundtrip");
    CHECK(got->risk==RISK_PRIVILEGED&&got->category==CAT_NETWORK,"risk/category roundtrip");
    CHECK(got->args[0].kind==ARG_TEXT&&strcmp(got->args[0].prompt_zh,"用户名")==0,"arg1 roundtrip");
    CHECK(got->args[2].kind==ARG_PORT&&strcmp(got->args[2].default_value,"22")==0,"arg3 roundtrip");

    CHECK(wb_command_action_add(&loaded,&loaded.sets[0],&simple,NULL)!=0,"system add rejected");
    CHECK(wb_command_action_update(&loaded,&loaded.sets[0],"system.one",&simple)!=0,"system update rejected");
    CHECK(wb_command_action_delete(&loaded,&loaded.sets[0],"system.one")!=0,"system delete rejected");

    CHECK(wb_command_action_delete(&loaded,re,"hello")==0,"delete action");
    CHECK(wb_command_action_find(re,"hello")==NULL,"deleted missing");

    for(int i=(int)wb_command_set_task_count(re);i<(int)WB_MAX_USER_ACTIONS;i++){
        char id[64];snprintf(id,sizeof(id),"a-%d",i);Task t=simple;t.id=id;
        CHECK(wb_command_action_add(&loaded,re,&t,NULL)==0,"fill action limit");
    }
    CHECK(wb_command_set_task_count(re)==WB_MAX_USER_ACTIONS,"action limit reached");
    Task extra=simple;extra.id="overflow";CHECK(wb_command_action_add(&loaded,re,&extra,NULL)!=0,"256 action limit enforced");

    wb_command_sets_free(&loaded);wb_command_sets_free(&store);rm_tree(tmp);
    puts("COMMAND SETS ACTIONS PASS");return 0;
}
