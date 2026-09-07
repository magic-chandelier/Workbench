#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "modules/command-sets/command_sets.h"

#define CHECK(x,msg) do { if(!(x)){ fprintf(stderr,"FAIL: %s\n",msg); return 1; } } while(0)

static int mode_is(const char *p, mode_t want){struct stat st;return stat(p,&st)==0&&((st.st_mode&0777)==want);}
static int join_path(char *out,size_t cap,const char *root,const char *leaf){
    size_t a=strlen(root),b=strlen(leaf);
    if(a+1+b+1>cap)return -1;
    memcpy(out,root,a);out[a]='/';memcpy(out+a+1,leaf,b+1);return 0;
}
static void rm_tree(const char *root){char cmd[8192];snprintf(cmd,sizeof(cmd),"rm -rf -- '%s'",root);system(cmd);}

int main(void){
    char tmp[]="/tmp/wb-command-sets-XXXXXX";
    CHECK(mkdtemp(tmp)!=NULL,"mkdtemp");
    char root[4096];snprintf(root,sizeof(root),"%s/command-sets",tmp);
    Task system_task={.id="sys",.title_zh="系统",.title_en="System",.desc_zh="",.desc_en="",.keywords="sys",.category=CAT_SYSTEM,.risk=RISK_NORMAL,.command="true",.source_id="linux-core"};
    CommandSetStore store;
    CHECK(wb_command_sets_init(&store,&system_task,1,root)==0,"store init");
    CHECK(store.count==1,"system set present");
    CHECK(mode_is(root,0700),"store directory mode 0700");

    CommandSet *set=NULL;
    CHECK(wb_command_set_create_user(&store,"nginx","Nginx\\Ops=一\n二","Nginx Ops","line1\nline2",&set)==0,"create user set");
    CHECK(set&&set->source==COMMAND_SET_USER&&wb_command_set_can_edit(set),"user set editable");
    CHECK(mode_is(set->path,0600),"set file mode 0600");
    CHECK(wb_command_set_update_metadata(&store,set,"Nginx 新","Nginx New","新描述","New description")==0,"update metadata");

    CommandSetStore loaded;
    CHECK(wb_command_sets_init(&loaded,&system_task,1,root)==0,"reload init");
    CHECK(wb_command_sets_load_user(&loaded)==0,"reload users");
    CHECK(loaded.count==2,"one user reloaded");
    CommandSet *got=wb_command_sets_find(&loaded,"nginx");
    CHECK(got!=NULL,"find reloaded");
    CHECK(strcmp(got->name_zh,"Nginx 新")==0,"name roundtrip");
    CHECK(strcmp(got->desc_zh,"新描述")==0,"description update roundtrip");

    char bad[4096];CHECK(join_path(bad,sizeof(bad),root,"bad.wbc")==0,"bad path");FILE*f=fopen(bad,"w");CHECK(f!=NULL,"bad fopen");fputs("garbage\n",f);fclose(f);chmod(bad,0600);
    char dup[4096];CHECK(join_path(dup,sizeof(dup),root,"dup.wbc")==0,"dup path");f=fopen(dup,"w");CHECK(f!=NULL,"dup fopen");fputs("WORKBENCH_COMMAND_SET=1\nid=nginx\nname_zh=Duplicate\nname_en=Duplicate\ndesc_zh=x\ndesc_en=x\n",f);fclose(f);chmod(dup,0600);
    wb_command_sets_free(&loaded);
    CHECK(wb_command_sets_init(&loaded,&system_task,1,root)==0,"reload2 init");
    CHECK(wb_command_sets_load_user(&loaded)==0,"bad files skipped");
    CHECK(loaded.count==2,"bad and duplicate skipped");

    CHECK(wb_command_set_create_user(&loaded,"linux","bad","bad","bad",NULL)!=0,"system id cannot shadow");
    CHECK(wb_command_set_create_user(&loaded,"nginx","bad","bad","bad",NULL)!=0,"duplicate id rejected");

    for(int i=1;i<(int)WB_MAX_USER_COMMAND_SETS;i++){
        char id[64];snprintf(id,sizeof(id),"set-%d",i);
        CHECK(wb_command_set_create_user(&loaded,id,id,id,"",NULL)==0,"fill user set limit");
    }
    CHECK(loaded.count==1+WB_MAX_USER_COMMAND_SETS,"user limit reached");
    CHECK(wb_command_set_create_user(&loaded,"overflow","overflow","overflow","",NULL)!=0,"64-set limit enforced");

    got=wb_command_sets_find(&loaded,"nginx");
    CHECK(got!=NULL,"nginx before delete");
    char saved_path[4096];snprintf(saved_path,sizeof(saved_path),"%s",got->path);
    CHECK(wb_command_set_delete_user(&loaded,got)==0,"delete user set");
    CHECK(access(saved_path,F_OK)!=0,"set file removed");

    wb_command_sets_free(&loaded);
    wb_command_sets_free(&store);
    rm_tree(tmp);
    puts("COMMAND SETS STORAGE PASS");
    return 0;
}
