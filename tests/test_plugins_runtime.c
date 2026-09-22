#define _POSIX_C_SOURCE 200809L
#include "modules/plugins/plugins.h"
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
static void make_plugin(const char *root,const char *id,const char *script){char d[1024],bin[1024],m[1024],run[1024],buf[4096];join(d,sizeof(d),root,id);mk(d);join(bin,sizeof(bin),d,"bin");mk(bin);join(run,sizeof(run),bin,"run");wr(run,script,0700);join(m,sizeof(m),d,"plugin.wbp");snprintf(buf,sizeof(buf),"WORKBENCH_PLUGIN=1\nformat=1\nid=%s\nversion=1\napi_min=1\napi_max=1\nname_zh=%s\nname_en=%s\ndescription_zh=x\ndescription_en=x\nentry=bin/run\n",id,id,id);wr(m,buf,0600);}
int main(void){
 char base[]="/tmp/wb-plugin-runtime-XXXXXX";assert(mkdtemp(base));char sys[1024],usr[1024];join(sys,sizeof(sys),base,"system");join(usr,sizeof(usr),base,"user");mk(sys);mk(usr);
 make_plugin(usr,"ok","#!/bin/sh\nexit 7\n");make_plugin(usr,"crash","#!/bin/sh\nkill -SEGV $$\n");make_plugin(usr,"remove","#!/bin/sh\nexit 0\n");
 WbPluginRegistry r;wb_plugin_registry_init(&r);assert(wb_plugin_registry_scan(&r,sys,usr)==0);
 WbPluginRunResult rr={0};char err[256]={0};const WbPlugin*p=wb_plugin_registry_find_active(&r,"ok");assert(p);assert(wb_plugin_launch(p,&rr,err,sizeof(err))==0);assert(rr.exited&&rr.exit_code==7&&!rr.signaled);
 p=wb_plugin_registry_find_active(&r,"crash");assert(p);memset(&rr,0,sizeof(rr));assert(wb_plugin_launch(p,&rr,err,sizeof(err))==0);assert(rr.signaled&&rr.term_signal==11);
 /* Revalidation: a manifest changed after scan must prevent launch. */
 char pm[1024],pd[1024];join(pd,sizeof(pd),usr,"ok");join(pm,sizeof(pm),pd,"plugin.wbp");wr(pm,"WORKBENCH_PLUGIN=1\nformat=1\nid=ok\nversion=1\napi_min=99\napi_max=99\nname_zh=ok\nname_en=ok\ndescription_zh=x\ndescription_en=x\nentry=bin/run\n",0600);p=wb_plugin_registry_find_active(&r,"ok");assert(p);assert(wb_plugin_launch(p,&rr,err,sizeof(err))!=0);
 /* Final entry symlink must be rejected. */
 char crashd[1024],run[1024],real[1024];join(crashd,sizeof(crashd),usr,"crash");join(run,sizeof(run),crashd,"bin/run");join(real,sizeof(real),base,"outside-run");wr(real,"#!/bin/sh\nexit 0\n",0700);assert(unlink(run)==0);assert(symlink(real,run)==0);p=wb_plugin_registry_find_active(&r,"crash");assert(p);assert(wb_plugin_launch(p,&rr,err,sizeof(err))!=0);
 /* Uninstall must not follow symlinks out of the plugin directory. */
 char outside[1024],remd[1024],linkp[1024];join(outside,sizeof(outside),base,"outside-data");wr(outside,"KEEP",0600);join(remd,sizeof(remd),usr,"remove");join(linkp,sizeof(linkp),remd,"outside-link");assert(symlink(outside,linkp)==0);p=wb_plugin_registry_find_active(&r,"remove");assert(p);assert(wb_plugin_uninstall(&r,p,err,sizeof(err))==0);assert(access(remd,F_OK)!=0);assert(access(outside,F_OK)==0);
 assert(wb_plugin_remove_system_by_id(sys,"../bad",err,sizeof(err))!=0);
 puts("PLUGINS RUNTIME PASS");return 0;
}
