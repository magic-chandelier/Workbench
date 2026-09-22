#define _POSIX_C_SOURCE 200809L
#include "modules/plugins/plugins.h"
#include "modules/extensions/extensions.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void mk(const char *p){if(mkdir(p,0700)&&errno!=EEXIST){perror(p);exit(1);}}
static void join(char *o,size_t c,const char*a,const char*b){size_t x=strlen(a),y=strlen(b);assert(x+1+y+1<=c);memcpy(o,a,x);o[x]='/';memcpy(o+x+1,b,y+1);}
static void wr(const char*p,const char*s,mode_t m){int f=open(p,O_WRONLY|O_CREAT|O_TRUNC,m);assert(f>=0);assert(write(f,s,strlen(s))==(ssize_t)strlen(s));close(f);chmod(p,m);}

int main(void){
    char base[]="/tmp/wb-plugin-actions-XXXXXX";assert(mkdtemp(base));
    char sys[1024],usr[1024],pd[1024],bin[1024],actions[1024],manifest[1024],entry[1024],desc[1024],out[1024],selected[1024];
    join(sys,sizeof(sys),base,"system");join(usr,sizeof(usr),base,"user");mk(sys);mk(usr);
    join(pd,sizeof(pd),usr,"plugin.archive");mk(pd);join(bin,sizeof(bin),pd,"bin");mk(bin);join(actions,sizeof(actions),pd,"file-actions");mk(actions);
    join(entry,sizeof(entry),bin,"archive");wr(entry,"#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$WB_TEST_OUT\"\n",0700);
    join(manifest,sizeof(manifest),pd,"plugin.wbp");wr(manifest,
        "WORKBENCH_PLUGIN=1\nformat=1\nid=plugin.archive\nversion=2.0\napi_min=2\napi_max=2\nname_zh=Archive\nname_en=Archive\ndescription_zh=x\ndescription_en=x\nentry=bin/archive\nfile_actions=file-actions\n",0600);
    join(desc,sizeof(desc),actions,"extract.wba");wr(desc,
        "WORKBENCH_FILE_ACTION=1\nformat=1\nid=extract-here\nname_zh=解压到这里\nname_en=Extract here\nentry=bin/archive\ntarget=file\nextensions=7z,zip,tar.gz\n",0600);
    WbPluginRegistry pr;wb_plugin_registry_init(&pr);assert(wb_plugin_registry_scan(&pr,sys,usr)==0);
    const WbPlugin *plugin=wb_plugin_registry_find_active(&pr,"plugin.archive");assert(plugin&&wb_plugin_supports_api(plugin,2));
    WbExtensionRegistry er;assert(wb_extension_registry_load_plugin_actions(&er,&pr)==0);assert(wb_extension_registry_count(&er)==1);
    const WbRegisteredExtensionAction *a=wb_extension_action_at(&er,0);assert(a&&!strcmp(a->id,"extract-here"));
    assert(wb_extension_action_matches_path(a,"/tmp/a.7z",0100000|0600));assert(!wb_extension_action_matches_path(a,"/tmp/a.txt",0100000|0600));
    char err[256]={0};assert(wb_extension_action_revalidate(plugin,a,err,sizeof(err))==0);
    join(out,sizeof(out),base,"argv.txt");assert(setenv("WB_TEST_OUT",out,1)==0);
    snprintf(selected,sizeof(selected),"%s/%s",base,"name with spaces;$(touch SHOULD_NOT_EXIST).7z");wr(selected,"x",0600);
    WbPluginRunResult rr={0};assert(wb_plugin_launch_file_action(plugin,a->entry,a->id,selected,&rr,err,sizeof(err))==0);assert(rr.exited&&rr.exit_code==0);
    FILE *f=fopen(out,"r");assert(f);char data[4096];size_t n=fread(data,1,sizeof(data)-1,f);fclose(f);data[n]='\0';
    char expected[4096];snprintf(expected,sizeof(expected),"--workbench-file-action\nextract-here\n--\n%s\n",selected);assert(!strcmp(data,expected));
    char injected[1024];join(injected,sizeof(injected),base,"SHOULD_NOT_EXIST");assert(access(injected,F_OK)!=0);
    wr(desc,"WORKBENCH_FILE_ACTION=1\nformat=1\nid=extract-here\nname_zh=解压到这里\nname_en=Extract here\nentry=bin/changed\ntarget=file\nextensions=7z\n",0600);
    assert(wb_extension_action_revalidate(plugin,a,err,sizeof(err))!=0);
    puts("PLUGIN FILE ACTIONS PASS");return 0;
}
