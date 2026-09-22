#define _POSIX_C_SOURCE 200809L
#include "modules/command-sets/command_sets.h"
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
static void wr(const char*p,const char*s){int f=open(p,O_WRONLY|O_CREAT|O_TRUNC,0600);assert(f>=0);assert(write(f,s,strlen(s))==(ssize_t)strlen(s));close(f);}
int main(void){
 static const Task sys[]={{"sys","Sys","Sys","d","d","sys",CAT_SYSTEM,RISK_NORMAL,"true","linux",{{0}}}};
 char base[]="/tmp/wb-plugin-cs-XXXXXX";assert(mkdtemp(base));char user[1024],plug[1024],file[1024];join(user,sizeof(user),base,"user");join(plug,sizeof(plug),base,"plugin-sets");mk(user);mk(plug);join(file,sizeof(file),plug,"tools.wbc");
 wr(file,"WORKBENCH_COMMAND_SET=1\nid=tools\nname_zh=插件工具\nname_en=Plugin Tools\ndesc_zh=插件只读\ndesc_en=Plugin read only\n[action]\nid=hello\ntitle_zh=问候\ntitle_en=Hello\ndesc_zh=x\ndesc_en=x\nkeywords=hello\ncommand={plugin_root}/bin/plugin-tool\ncategory=0\nrisk=0\n");
 CommandSetStore s;assert(wb_command_sets_init(&s,sys,1,user)==0);assert(wb_command_sets_load_plugin_dir_at_root(&s,"demo",plug,base)==0);assert(s.count==2);
 const CommandSet *p=wb_command_sets_find_const(&s,"plugin.demo.tools");assert(p);assert(p->source==COMMAND_SET_PLUGIN);assert(p->read_only);assert(!wb_command_set_can_edit(p));assert(wb_command_set_task_count(p)==1);const Task*t=wb_command_set_task_at(p,0);assert(t&&!strcmp(t->id,"hello"));assert(t->source_id&&!strcmp(t->source_id,"plugin.demo"));assert(!strstr(t->command,"{plugin_root}"));char expected[1400];snprintf(expected,sizeof(expected),"'%s'/bin/plugin-tool",base);assert(!strcmp(t->command,expected));
 assert(wb_command_set_rename((CommandSet*)p,"x","x")!=0);assert(wb_command_set_mark_deleted((CommandSet*)p)!=0);
 wb_command_sets_remove_plugins(&s);assert(s.count==1);assert(wb_command_sets_find_const(&s,"linux"));
 wb_command_sets_free(&s);puts("PLUGIN COMMAND SETS PASS");return 0;
}
