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

static void mk(const char*p){if(mkdir(p,0700)&&errno!=EEXIST){perror(p);exit(1);}}
static void join(char*o,size_t c,const char*a,const char*b){size_t x=strlen(a),y=strlen(b);assert(x+1+y+1<=c);memcpy(o,a,x);o[x]='/';memcpy(o+x+1,b,y+1);}
static void wr(const char*p,const char*s,mode_t m){int f=open(p,O_WRONLY|O_CREAT|O_TRUNC,m);assert(f>=0);assert(write(f,s,strlen(s))==(ssize_t)strlen(s));close(f);chmod(p,m);}
static void make_plugin(const char *root,const char *id){char d[1024],bin[1024],m[1024],run[1024],buf[4096];join(d,sizeof(d),root,id);mk(d);join(bin,sizeof(bin),d,"bin");mk(bin);join(run,sizeof(run),bin,"run");wr(run,"#!/bin/sh\nexit 0\n",0700);join(m,sizeof(m),d,"plugin.wbp");snprintf(buf,sizeof(buf),"WORKBENCH_PLUGIN=1\nformat=1\nid=%s\nversion=1\napi_min=1\napi_max=1\nname_zh=%s\nname_en=%s\ndescription_zh=x\ndescription_en=x\nentry=bin/run\n",id,id,id);wr(m,buf,0600);}
static const WbPlugin *find_any(const WbPluginRegistry *r,const char *id,WbPluginSource src){for(size_t i=0;i<r->count;i++)if(!strcmp(r->items[i].id,id)&&r->items[i].source==src)return &r->items[i];return NULL;}
int main(void){
    char base[]="/tmp/wb-plugin-toggle-XXXXXX";assert(mkdtemp(base));char sys[1024],usr[1024];join(sys,sizeof(sys),base,"system");join(usr,sizeof(usr),base,"user");mk(sys);mk(usr);make_plugin(sys,"demo");make_plugin(usr,"demo");
    WbPluginRegistry r;wb_plugin_registry_init(&r);assert(wb_plugin_registry_scan(&r,sys,usr)==0);const WbPlugin *p=wb_plugin_registry_find_active(&r,"demo");assert(p&&p->source==WB_PLUGIN_USER);
    WbPlugin chosen=*p;char err[256]={0};assert(wb_plugin_set_enabled(&r,&chosen,0,err,sizeof(err))==0);assert(wb_plugin_registry_scan(&r,sys,usr)==0);assert(!wb_plugin_registry_find_active(&r,"demo"));p=find_any(&r,"demo",WB_PLUGIN_USER);assert(p&&p->status==WB_PLUGIN_DISABLED&&!p->enabled);const WbPlugin *sp=find_any(&r,"demo",WB_PLUGIN_SYSTEM);assert(sp&&sp->status==WB_PLUGIN_SHADOWED);
    WbPluginRunResult rr={0};assert(wb_plugin_launch(p,&rr,err,sizeof(err))!=0);assert(strstr(err,"disabled")!=NULL);
    chosen=*p;assert(wb_plugin_set_enabled(&r,&chosen,1,err,sizeof(err))==0);assert(wb_plugin_registry_scan(&r,sys,usr)==0);p=wb_plugin_registry_find_active(&r,"demo");assert(p&&p->source==WB_PLUGIN_USER&&p->enabled);
    chosen=*p;snprintf(chosen.root,sizeof(chosen.root),"%s/demo-elsewhere",usr);assert(wb_plugin_set_enabled(&r,&chosen,0,err,sizeof(err))!=0);
    puts("PLUGIN ENABLE DISABLE PASS");return 0;
}
