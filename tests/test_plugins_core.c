#define _POSIX_C_SOURCE 200809L
#include "modules/plugins/plugins.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void die(const char *s){perror(s);exit(1);} 
static void mk(const char *p){if(mkdir(p,0700)&&errno!=EEXIST)die(p);} 
static void write_file(const char *p,const char *s,mode_t mode){int fd=open(p,O_WRONLY|O_CREAT|O_TRUNC,mode);if(fd<0)die(p);size_t n=strlen(s);if(write(fd,s,n)!=(ssize_t)n)die("write");close(fd);chmod(p,mode);} 
static void join(char *out,size_t cap,const char *a,const char *b){size_t na=strlen(a),nb=strlen(b);assert(na+1+nb+1<=cap);memcpy(out,a,na);out[na]='/';memcpy(out+na+1,b,nb+1);}
static void manifest(const char *root,const char *id,const char *version,const char *extra){
 char dir[1024],path[1024],buf[4096];join(dir,sizeof(dir),root,id);mk(dir);join(path,sizeof(path),dir,"plugin.wbp");
 snprintf(buf,sizeof(buf),"WORKBENCH_PLUGIN=1\nformat=1\nid=%s\nversion=%s\napi_min=1\napi_max=1\nname_zh=%s\nname_en=%s\ndescription_zh=test\ndescription_en=test\n%s",id,version,id,id,extra?extra:"");write_file(path,buf,0600);
}
int main(void){
 char base[]="/tmp/wb-plugin-test-XXXXXX";assert(mkdtemp(base));char sys[1024],usr[1024];snprintf(sys,sizeof(sys),"%s/system",base);snprintf(usr,sizeof(usr),"%s/user",base);mk(sys);mk(usr);
 manifest(sys,"demo","1.0.0","entry=bin/run\n");manifest(usr,"demo","2.0.0","entry=bin/run\n");
 manifest(sys,"stable","1.0.0","");
 manifest(usr,"badapi","1.0.0","api_min=99\n");
 char bad[1024];join(bad,sizeof(bad),usr,"broken");mk(bad);char bp[1024];join(bp,sizeof(bp),bad,"plugin.wbp");write_file(bp,"WORKBENCH_PLUGIN=1\nid=broken\n",0600);
 char hidden[1024];join(hidden,sizeof(hidden),usr,".wb-removing-old-1");mk(hidden);
 char real[1024],linkp[1024];join(real,sizeof(real),base,"real");mk(real);join(linkp,sizeof(linkp),usr,"linkplug");assert(symlink(real,linkp)==0);
 WbPluginRegistry r;wb_plugin_registry_init(&r);assert(wb_plugin_registry_scan(&r,sys,usr)==0);
 const WbPlugin *p=wb_plugin_registry_find_active(&r,"demo");assert(p&&p->source==WB_PLUGIN_USER&&!strcmp(p->version,"2.0.0"));
 assert(wb_plugin_registry_find_active(&r,"stable"));assert(!wb_plugin_registry_find_active(&r,"broken"));assert(!wb_plugin_registry_find_active(&r,"badapi"));assert(!wb_plugin_registry_find_active(&r,"linkplug"));
 assert(wb_plugin_registry_active_count(&r)==2);
 char user_demo[1024];join(user_demo,sizeof(user_demo),usr,"demo");assert(rename(user_demo,hidden)==0);assert(wb_plugin_registry_scan(&r,sys,usr)==0);p=wb_plugin_registry_find_active(&r,"demo");assert(p&&p->source==WB_PLUGIN_SYSTEM&&!strcmp(p->version,"1.0.0"));
 wb_plugin_registry_clear(&r);printf("PLUGINS CORE PASS\n");return 0;
}
