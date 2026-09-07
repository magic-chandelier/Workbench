#define _POSIX_C_SOURCE 200809L
#include "command_sets.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct WbUserTask {
    Task task;
    char id[64];
    char title_zh[128];
    char title_en[128];
    char desc_zh[256];
    char desc_en[256];
    char keywords[256];
    char command[2048];
    char source_id[64];
    char arg_prompt_zh[WB_MAX_ARGS][128];
    char arg_prompt_en[WB_MAX_ARGS][128];
    char arg_default[WB_MAX_ARGS][256];
};

static int copy_text(char *dst,size_t cap,const char *src){
    if(!dst||cap==0||!src)return -1;
    size_t n=strlen(src);
    if(n>=cap)return -1;
    memcpy(dst,src,n+1);
    return 0;
}

static int valid_id(const char *id){
    if(!id||!*id||strlen(id)>=64)return 0;
    for(const unsigned char *p=(const unsigned char*)id;*p;p++)
        if(!(isalnum(*p)||*p=='-'||*p=='_'||*p=='.'))return 0;
    return strcmp(id,".")&&strcmp(id,"..");
}

static int mkdir_path_0700(const char *path){
    char buf[4096];
    if(copy_text(buf,sizeof(buf),path))return -1;
    size_t len=strlen(buf);
    if(!len)return -1;
    for(size_t i=1;i<=len;i++){
        if(buf[i]!='/'&&buf[i]!='\0')continue;
        char hold=buf[i];buf[i]='\0';
        if(*buf&&mkdir(buf,0700)&&errno!=EEXIST){buf[i]=hold;return -1;}
        buf[i]=hold;
    }
    if(chmod(path,0700)&&errno!=ENOENT)return -1;
    return 0;
}

static int escape_value(const char *src,char *out,size_t cap){
    size_t u=0;
    if(!src||!out||!cap)return -1;
    for(const unsigned char*p=(const unsigned char*)src;*p;p++){
        const char *rep=NULL;char tmp[3]={0};
        if(*p=='\\')rep="\\\\";
        else if(*p=='\n')rep="\\n";
        else if(*p=='\r')rep="\\r";
        else if(*p=='\t')rep="\\t";
        else if(*p=='=')rep="\\e";
        else if(*p<0x20||*p==0x7f)return -1;
        else {tmp[0]=(char)*p;rep=tmp;}
        size_t n=strlen(rep);if(u+n+1>cap)return -1;memcpy(out+u,rep,n);u+=n;
    }
    out[u]='\0';return 0;
}

static int unescape_value(const char *src,char *out,size_t cap){
    size_t u=0;
    if(!src||!out||!cap)return -1;
    for(size_t i=0;src[i];i++){
        unsigned char c=(unsigned char)src[i];
        if(c=='\\'){
            char n=src[++i];if(!n)return -1;
            if(n=='n')c='\n';else if(n=='r')c='\r';else if(n=='t')c='\t';else if(n=='e')c='=';else if(n=='\\')c='\\';else return -1;
        }
        if(u+2>cap)return -1;
        out[u++]=(char)c;
    }
    out[u]='\0';return 0;
}

int wb_command_set_init_system_linux(CommandSet *set,const Task *tasks,size_t task_count){
    if(!set||(!tasks&&task_count))return -1;
    memset(set,0,sizeof(*set));
    if(copy_text(set->id,sizeof(set->id),"linux")||
       copy_text(set->name_zh,sizeof(set->name_zh),"Linux 指令集")||
       copy_text(set->name_en,sizeof(set->name_en),"Linux Commands")||
       copy_text(set->desc_zh,sizeof(set->desc_zh),"Workbench 系统预制 Linux 指令集")||
       copy_text(set->desc_en,sizeof(set->desc_en),"Workbench built-in Linux command set"))return -1;
    set->source=COMMAND_SET_SYSTEM;
    set->read_only=1;
    set->system_tasks=tasks;
    set->system_task_count=task_count;
    return 0;
}

size_t wb_command_set_task_count(const CommandSet *set){
    if(!set)return 0;
    return set->source==COMMAND_SET_SYSTEM?set->system_task_count:set->user_task_count;
}

const Task *wb_command_set_task_at(const CommandSet *set,size_t index){
    if(!set)return NULL;
    if(set->source==COMMAND_SET_SYSTEM)return index<set->system_task_count?&set->system_tasks[index]:NULL;
    if(set->source==COMMAND_SET_USER&&index<set->user_task_count)return &set->user_tasks[index].task;
    return NULL;
}

int wb_command_set_can_edit(const CommandSet *set){return set&&set->source==COMMAND_SET_USER&&!set->read_only&&!set->deleted;}

int wb_command_set_rename(CommandSet *set,const char *name_zh,const char *name_en){
    if(!wb_command_set_can_edit(set)||!name_zh||!*name_zh||!name_en||!*name_en)return -1;
    if(copy_text(set->name_zh,sizeof(set->name_zh),name_zh)||copy_text(set->name_en,sizeof(set->name_en),name_en))return -1;
    return 0;
}

int wb_command_set_mark_deleted(CommandSet *set){
    if(!wb_command_set_can_edit(set))return -1;
    set->deleted=1;
    return 0;
}

void wb_command_set_destroy(CommandSet *set){
    if(!set)return;
    free(set->user_tasks);
    memset(set,0,sizeof(*set));
}

int wb_command_sets_default_root(char *out,size_t cap){
    const char *home=getenv("HOME");
    if(!home||!*home)return -1;
    int n=snprintf(out,cap,"%s/.local/share/workbench/command-sets",home);
    return n<0||(size_t)n>=cap?-1:0;
}

int wb_command_sets_init(CommandSetStore *store,const Task *system_tasks,size_t system_task_count,const char *root){
    if(!store||!root||!*root)return -1;
    memset(store,0,sizeof(*store));
    if(copy_text(store->root,sizeof(store->root),root)||mkdir_path_0700(store->root))return -1;
    if(wb_command_set_init_system_linux(&store->sets[0],system_tasks,system_task_count))return -1;
    store->count=1;
    return 0;
}

CommandSet *wb_command_sets_find(CommandSetStore *store,const char *id){
    if(!store||!id)return NULL;
    for(size_t i=0;i<store->count;i++)if(!store->sets[i].deleted&&!strcmp(store->sets[i].id,id))return &store->sets[i];
    return NULL;
}
const CommandSet *wb_command_sets_find_const(const CommandSetStore *store,const char *id){return wb_command_sets_find((CommandSetStore*)store,id);}

static int set_path(const CommandSetStore *store,const char *id,char *out,size_t cap){
    if(!valid_id(id))return -1;
    int n=snprintf(out,cap,"%s/%s.wbc",store->root,id);
    return n<0||(size_t)n>=cap?-1:0;
}

static int template_has_placeholder_local(const char *cmd,int n){
    char p[4]={'{',(char)('0'+n),'}','\0'};
    return cmd&&strstr(cmd,p)!=NULL;
}

static int user_task_valid(const Task *task){
    if(!task||!valid_id(task->id)||!task->title_zh||!*task->title_zh||!task->title_en||!*task->title_en||
       !task->desc_zh||!task->desc_en||!task->keywords||!task->command||!*task->command)return 0;
    if(task->category<0||task->category>=CAT_COUNT||task->risk<RISK_NORMAL||task->risk>RISK_PRIVILEGED)return 0;
    for(int i=0;i<WB_MAX_ARGS;i++){
        ArgKind k=task->args[i].kind;
        if(k<ARG_NONE||k>ARG_PID)return 0;
        if(template_has_placeholder_local(task->command,i+1)!=(k!=ARG_NONE))return 0;
        if(k!=ARG_NONE&&(!task->args[i].prompt_zh||!*task->args[i].prompt_zh||!task->args[i].prompt_en||!*task->args[i].prompt_en||!task->args[i].default_value))return 0;
    }
    return 1;
}

static void rebind_owned_task(WbUserTask *t){
    if(!t)return;
    t->task.id=t->id;t->task.title_zh=t->title_zh;t->task.title_en=t->title_en;t->task.desc_zh=t->desc_zh;t->task.desc_en=t->desc_en;
    t->task.keywords=t->keywords;t->task.command=t->command;t->task.source_id=t->source_id;
    for(int i=0;i<WB_MAX_ARGS;i++){
        if(t->task.args[i].kind==ARG_NONE){t->task.args[i].prompt_zh=NULL;t->task.args[i].prompt_en=NULL;t->task.args[i].default_value=NULL;}
        else {t->task.args[i].prompt_zh=t->arg_prompt_zh[i];t->task.args[i].prompt_en=t->arg_prompt_en[i];t->task.args[i].default_value=t->arg_default[i];}
    }
}

static int owned_task_from_task(WbUserTask *dst,const Task *src){
    if(!dst||!user_task_valid(src))return -1;
    memset(dst,0,sizeof(*dst));
    if(copy_text(dst->id,sizeof(dst->id),src->id)||copy_text(dst->title_zh,sizeof(dst->title_zh),src->title_zh)||
       copy_text(dst->title_en,sizeof(dst->title_en),src->title_en)||copy_text(dst->desc_zh,sizeof(dst->desc_zh),src->desc_zh)||
       copy_text(dst->desc_en,sizeof(dst->desc_en),src->desc_en)||copy_text(dst->keywords,sizeof(dst->keywords),src->keywords)||
       copy_text(dst->command,sizeof(dst->command),src->command)||copy_text(dst->source_id,sizeof(dst->source_id),"user"))return -1;
    dst->task=(Task){dst->id,dst->title_zh,dst->title_en,dst->desc_zh,dst->desc_en,dst->keywords,src->category,src->risk,dst->command,dst->source_id,{{0}}};
    for(int i=0;i<WB_MAX_ARGS;i++){
        dst->task.args[i].kind=src->args[i].kind;
        if(src->args[i].kind==ARG_NONE)continue;
        if(copy_text(dst->arg_prompt_zh[i],sizeof(dst->arg_prompt_zh[i]),src->args[i].prompt_zh)||
           copy_text(dst->arg_prompt_en[i],sizeof(dst->arg_prompt_en[i]),src->args[i].prompt_en)||
           copy_text(dst->arg_default[i],sizeof(dst->arg_default[i]),src->args[i].default_value))return -1;
        dst->task.args[i].prompt_zh=dst->arg_prompt_zh[i];
        dst->task.args[i].prompt_en=dst->arg_prompt_en[i];
        dst->task.args[i].default_value=dst->arg_default[i];
    }
    rebind_owned_task(dst);
    return 0;
}

static int reserve_user_tasks(CommandSet *set,size_t need){
    if(need>WB_MAX_USER_ACTIONS)return -1;
    if(need<=set->user_task_capacity)return 0;
    size_t cap=set->user_task_capacity?set->user_task_capacity*2:8;
    while(cap<need)cap*=2;
    if(cap>WB_MAX_USER_ACTIONS)cap=WB_MAX_USER_ACTIONS;
    WbUserTask *p=realloc(set->user_tasks,cap*sizeof(*p));
    if(!p)return -1;
    set->user_tasks=p;set->user_task_capacity=cap;
    for(size_t i=0;i<set->user_task_count;i++)rebind_owned_task(&set->user_tasks[i]);
    return 0;
}

static int write_escaped_line(FILE *f,const char *key,const char *value){
    char buf[4096];if(escape_value(value?value:"",buf,sizeof(buf)))return -1;
    return fprintf(f,"%s=%s\n",key,buf)<0?-1:0;
}

static int write_metadata(FILE *f,const CommandSet *set){
    char a[1024],b[1024],c[2048],d[2048];
    if(escape_value(set->name_zh,a,sizeof(a))||escape_value(set->name_en,b,sizeof(b))||escape_value(set->desc_zh,c,sizeof(c))||escape_value(set->desc_en,d,sizeof(d)))return -1;
    if(fprintf(f,"WORKBENCH_COMMAND_SET=1\nid=%s\nname_zh=%s\nname_en=%s\ndesc_zh=%s\ndesc_en=%s\n",set->id,a,b,c,d)<0)return -1;
    for(size_t i=0;i<set->user_task_count;i++){
        const Task *t=&set->user_tasks[i].task;
        if(fputs("\n[action]\n",f)==EOF)return -1;
        if(write_escaped_line(f,"id",t->id)||write_escaped_line(f,"title_zh",t->title_zh)||write_escaped_line(f,"title_en",t->title_en)||
           write_escaped_line(f,"desc_zh",t->desc_zh)||write_escaped_line(f,"desc_en",t->desc_en)||write_escaped_line(f,"keywords",t->keywords)||
           fprintf(f,"category=%d\nrisk=%d\n",(int)t->category,(int)t->risk)<0||write_escaped_line(f,"command",t->command))return -1;
        for(int ai=0;ai<WB_MAX_ARGS;ai++){
            char key[64];
            if(fprintf(f,"arg%d_kind=%d\n",ai+1,(int)t->args[ai].kind)<0)return -1;
            snprintf(key,sizeof(key),"arg%d_prompt_zh",ai+1);if(write_escaped_line(f,key,t->args[ai].kind==ARG_NONE?"":t->args[ai].prompt_zh))return -1;
            snprintf(key,sizeof(key),"arg%d_prompt_en",ai+1);if(write_escaped_line(f,key,t->args[ai].kind==ARG_NONE?"":t->args[ai].prompt_en))return -1;
            snprintf(key,sizeof(key),"arg%d_default",ai+1);if(write_escaped_line(f,key,t->args[ai].kind==ARG_NONE?"":t->args[ai].default_value))return -1;
        }
    }
    return 0;
}

int wb_command_set_save(CommandSetStore *store,CommandSet *set){
    if(!store||!set||!wb_command_set_can_edit(set))return -1;
    char path[4096],tmp[4096];
    if(set_path(store,set->id,path,sizeof(path)))return -1;
    int n=snprintf(tmp,sizeof(tmp),"%s.tmp.%ld",path,(long)getpid());if(n<0||(size_t)n>=sizeof(tmp))return -1;
    int fd=open(tmp,O_CREAT|O_TRUNC|O_WRONLY,0600);if(fd<0)return -1;
    if(fchmod(fd,0600)){close(fd);unlink(tmp);return -1;}
    FILE*f=fdopen(fd,"w");if(!f){close(fd);unlink(tmp);return -1;}
    int ok=write_metadata(f,set)==0&&fflush(f)==0&&fsync(fd)==0&&fclose(f)==0;
    if(!ok){unlink(tmp);return -1;}
    if(rename(tmp,path)){unlink(tmp);return -1;}
    if(copy_text(set->path,sizeof(set->path),path))return -1;
    return 0;
}

int wb_command_set_update_metadata(CommandSetStore *store,CommandSet *set,const char *name_zh,const char *name_en,const char *desc_zh,const char *desc_en){
    if(!store||!set||!wb_command_set_can_edit(set)||!name_zh||!*name_zh||!name_en||!*name_en||!desc_zh||!desc_en)return -1;
    char old_name_zh[128],old_name_en[128],old_desc_zh[256],old_desc_en[256];
    memcpy(old_name_zh,set->name_zh,sizeof(old_name_zh));memcpy(old_name_en,set->name_en,sizeof(old_name_en));
    memcpy(old_desc_zh,set->desc_zh,sizeof(old_desc_zh));memcpy(old_desc_en,set->desc_en,sizeof(old_desc_en));
    if(copy_text(set->name_zh,sizeof(set->name_zh),name_zh)||copy_text(set->name_en,sizeof(set->name_en),name_en)||copy_text(set->desc_zh,sizeof(set->desc_zh),desc_zh)||copy_text(set->desc_en,sizeof(set->desc_en),desc_en)){
        memcpy(set->name_zh,old_name_zh,sizeof(old_name_zh));memcpy(set->name_en,old_name_en,sizeof(old_name_en));memcpy(set->desc_zh,old_desc_zh,sizeof(old_desc_zh));memcpy(set->desc_en,old_desc_en,sizeof(old_desc_en));return -1;
    }
    if(wb_command_set_save(store,set)){
        memcpy(set->name_zh,old_name_zh,sizeof(old_name_zh));memcpy(set->name_en,old_name_en,sizeof(old_name_en));memcpy(set->desc_zh,old_desc_zh,sizeof(old_desc_zh));memcpy(set->desc_en,old_desc_en,sizeof(old_desc_en));return -1;
    }
    return 0;
}

int wb_command_set_create_user(CommandSetStore *store,const char *id,const char *name_zh,const char *name_en,const char *description,CommandSet **out){
    if(out)*out=NULL;
    if(!store||!valid_id(id)||!name_zh||!*name_zh||!name_en||!*name_en||!description)return -1;
    if(store->count>=WB_MAX_USER_COMMAND_SETS+1||wb_command_sets_find(store,id))return -1;
    CommandSet *set=&store->sets[store->count];memset(set,0,sizeof(*set));
    if(copy_text(set->id,sizeof(set->id),id)||copy_text(set->name_zh,sizeof(set->name_zh),name_zh)||copy_text(set->name_en,sizeof(set->name_en),name_en)||copy_text(set->desc_zh,sizeof(set->desc_zh),description)||copy_text(set->desc_en,sizeof(set->desc_en),description)){memset(set,0,sizeof(*set));return -1;}
    set->source=COMMAND_SET_USER;set->read_only=0;
    if(wb_command_set_save(store,set)){memset(set,0,sizeof(*set));return -1;}
    store->count++;if(out)*out=set;return 0;
}

static int parse_int_range(const char *s,int lo,int hi,int *out){
    if(!s||!*s)return -1;
    char *end=NULL;long v=strtol(s,&end,10);
    if(!end||*end||v<lo||v>hi)return -1;
    *out=(int)v;
    return 0;
}

static int finalize_parsed_action(CommandSet *out,WbUserTask *cur,int have_action){
    if(!have_action)return 0;
    if(!user_task_valid(&cur->task))return -1;
    if(wb_command_action_find(out,cur->task.id))return -1;
    if(reserve_user_tasks(out,out->user_task_count+1))return -1;
    out->user_tasks[out->user_task_count++]=*cur;
    rebind_owned_task(&out->user_tasks[out->user_task_count-1]);
    memset(cur,0,sizeof(*cur));return 0;
}

static int parse_metadata_file(const char *path,const char *expected_id,CommandSet *out){
    FILE*f=fopen(path,"r");if(!f)return -1;
    char line[8192];int version=0,have_id=0,have_zh=0,have_en=0,have_dzh=0,have_den=0,in_action=0,action_seen=0;memset(out,0,sizeof(*out));
    WbUserTask cur={0};
    while(fgets(line,sizeof(line),f)){
        size_t n=strlen(line);while(n&&(line[n-1]=='\n'||line[n-1]=='\r'))line[--n]='\0';
        if(!strcmp(line,"[action]")){
            if(finalize_parsed_action(out,&cur,action_seen)){fclose(f);wb_command_set_destroy(out);return -1;}
            in_action=1;action_seen=1;cur.task.source_id=cur.source_id;copy_text(cur.source_id,sizeof(cur.source_id),"user");continue;
        }
        char *eq=strchr(line,'=');if(!eq)continue;*eq++='\0';
        if(!in_action){
            if(!strcmp(line,"WORKBENCH_COMMAND_SET")){if(strcmp(eq,"1")){fclose(f);wb_command_set_destroy(out);return -1;}version=1;continue;}
            if(!strcmp(line,"id")){if(!valid_id(eq)||copy_text(out->id,sizeof(out->id),eq)){fclose(f);wb_command_set_destroy(out);return -1;}have_id=1;}
            else if(!strcmp(line,"name_zh")){if(unescape_value(eq,out->name_zh,sizeof(out->name_zh))){fclose(f);wb_command_set_destroy(out);return -1;}have_zh=1;}
            else if(!strcmp(line,"name_en")){if(unescape_value(eq,out->name_en,sizeof(out->name_en))){fclose(f);wb_command_set_destroy(out);return -1;}have_en=1;}
            else if(!strcmp(line,"desc_zh")){if(unescape_value(eq,out->desc_zh,sizeof(out->desc_zh))){fclose(f);wb_command_set_destroy(out);return -1;}have_dzh=1;}
            else if(!strcmp(line,"desc_en")){if(unescape_value(eq,out->desc_en,sizeof(out->desc_en))){fclose(f);wb_command_set_destroy(out);return -1;}have_den=1;}
            continue;
        }
        if(!strcmp(line,"id")){if(unescape_value(eq,cur.id,sizeof(cur.id)))goto bad;cur.task.id=cur.id;}
        else if(!strcmp(line,"title_zh")){if(unescape_value(eq,cur.title_zh,sizeof(cur.title_zh)))goto bad;cur.task.title_zh=cur.title_zh;}
        else if(!strcmp(line,"title_en")){if(unescape_value(eq,cur.title_en,sizeof(cur.title_en)))goto bad;cur.task.title_en=cur.title_en;}
        else if(!strcmp(line,"desc_zh")){if(unescape_value(eq,cur.desc_zh,sizeof(cur.desc_zh)))goto bad;cur.task.desc_zh=cur.desc_zh;}
        else if(!strcmp(line,"desc_en")){if(unescape_value(eq,cur.desc_en,sizeof(cur.desc_en)))goto bad;cur.task.desc_en=cur.desc_en;}
        else if(!strcmp(line,"keywords")){if(unescape_value(eq,cur.keywords,sizeof(cur.keywords)))goto bad;cur.task.keywords=cur.keywords;}
        else if(!strcmp(line,"command")){if(unescape_value(eq,cur.command,sizeof(cur.command)))goto bad;cur.task.command=cur.command;}
        else if(!strcmp(line,"category")){int v;if(parse_int_range(eq,0,CAT_COUNT-1,&v))goto bad;cur.task.category=(Category)v;}
        else if(!strcmp(line,"risk")){int v;if(parse_int_range(eq,RISK_NORMAL,RISK_PRIVILEGED,&v))goto bad;cur.task.risk=(Risk)v;}
        else if(!strncmp(line,"arg",3)&&isdigit((unsigned char)line[3])&&line[4]=='_'){
            int ai=line[3]-'1';if(ai<0||ai>=WB_MAX_ARGS)goto bad;const char *field=line+5;
            if(!strcmp(field,"kind")){int v;if(parse_int_range(eq,ARG_NONE,ARG_PID,&v))goto bad;cur.task.args[ai].kind=(ArgKind)v;}
            else if(!strcmp(field,"prompt_zh")){if(unescape_value(eq,cur.arg_prompt_zh[ai],sizeof(cur.arg_prompt_zh[ai])))goto bad;cur.task.args[ai].prompt_zh=cur.arg_prompt_zh[ai];}
            else if(!strcmp(field,"prompt_en")){if(unescape_value(eq,cur.arg_prompt_en[ai],sizeof(cur.arg_prompt_en[ai])))goto bad;cur.task.args[ai].prompt_en=cur.arg_prompt_en[ai];}
            else if(!strcmp(field,"default")){if(unescape_value(eq,cur.arg_default[ai],sizeof(cur.arg_default[ai])))goto bad;cur.task.args[ai].default_value=cur.arg_default[ai];}
        }
    }
    if(finalize_parsed_action(out,&cur,action_seen))goto bad_after_close;
    fclose(f);
    if(!version||!have_id||!have_zh||!have_en||!have_dzh||!have_den||strcmp(out->id,expected_id)){wb_command_set_destroy(out);return -1;}
    out->source=COMMAND_SET_USER;out->read_only=0;if(copy_text(out->path,sizeof(out->path),path)){wb_command_set_destroy(out);return -1;}return 0;
bad:
    fclose(f);wb_command_set_destroy(out);return -1;
bad_after_close:
    fclose(f);wb_command_set_destroy(out);return -1;
}

int wb_command_sets_load_user(CommandSetStore *store){
    if(!store)return -1;
    DIR*d=opendir(store->root);if(!d)return -1;struct dirent*de;
    while((de=readdir(d))){
        size_t n=strlen(de->d_name);if(n<=4||strcmp(de->d_name+n-4,".wbc"))continue;
        if(store->count>=WB_MAX_USER_COMMAND_SETS+1)break;
        char id[64];if(n-4>=sizeof(id))continue;memcpy(id,de->d_name,n-4);id[n-4]='\0';if(!valid_id(id)||!strcmp(id,"linux")||wb_command_sets_find(store,id))continue;
        char path[4096];if(set_path(store,id,path,sizeof(path)))continue;
        struct stat st;if(lstat(path,&st)||!S_ISREG(st.st_mode))continue;
        CommandSet parsed;if(parse_metadata_file(path,id,&parsed))continue;
        store->sets[store->count++]=parsed;
    }
    closedir(d);return 0;
}

int wb_command_set_delete_user(CommandSetStore *store,CommandSet *set){
    if(!store||!set||!wb_command_set_can_edit(set))return -1;
    size_t idx=(size_t)(set-store->sets);if(idx==0||idx>=store->count)return -1;
    if(set->path[0]&&unlink(set->path)&&errno!=ENOENT)return -1;
    wb_command_set_destroy(set);
    for(size_t i=idx+1;i<store->count;i++)store->sets[i-1]=store->sets[i];
    memset(&store->sets[store->count-1],0,sizeof(store->sets[0]));store->count--;return 0;
}

const Task *wb_command_action_find(const CommandSet *set,const char *id){
    if(!set||!id)return NULL;
    size_t n=wb_command_set_task_count(set);
    for(size_t i=0;i<n;i++){const Task *t=wb_command_set_task_at(set,i);if(t&&t->id&&!strcmp(t->id,id))return t;}
    return NULL;
}

int wb_command_action_add(CommandSetStore *store,CommandSet *set,const Task *task,size_t *out_index){
    if(out_index)*out_index=0;
    if(!store||!set||!wb_command_set_can_edit(set)||!user_task_valid(task)||wb_command_action_find(set,task->id)||set->user_task_count>=WB_MAX_USER_ACTIONS)return -1;
    if(reserve_user_tasks(set,set->user_task_count+1))return -1;
    size_t idx=set->user_task_count;
    if(owned_task_from_task(&set->user_tasks[idx],task))return -1;
    set->user_task_count++;
    if(wb_command_set_save(store,set)){set->user_task_count--;memset(&set->user_tasks[idx],0,sizeof(set->user_tasks[idx]));return -1;}
    if(out_index)*out_index=idx;
    return 0;
}

int wb_command_action_update(CommandSetStore *store,CommandSet *set,const char *existing_id,const Task *task){
    if(!store||!set||!wb_command_set_can_edit(set)||!existing_id||!user_task_valid(task))return -1;
    size_t idx=set->user_task_count;
    for(size_t i=0;i<set->user_task_count;i++)if(!strcmp(set->user_tasks[i].task.id,existing_id)){idx=i;break;}
    if(idx==set->user_task_count)return -1;
    const Task *dup=wb_command_action_find(set,task->id);if(dup&&dup!=&set->user_tasks[idx].task)return -1;
    WbUserTask old=set->user_tasks[idx],next;
    if(owned_task_from_task(&next,task))return -1;
    set->user_tasks[idx]=next;rebind_owned_task(&set->user_tasks[idx]);
    if(wb_command_set_save(store,set)){set->user_tasks[idx]=old;rebind_owned_task(&set->user_tasks[idx]);return -1;}
    return 0;
}

int wb_command_action_delete(CommandSetStore *store,CommandSet *set,const char *id){
    if(!store||!set||!wb_command_set_can_edit(set)||!id)return -1;
    size_t idx=set->user_task_count;
    for(size_t i=0;i<set->user_task_count;i++)if(!strcmp(set->user_tasks[i].task.id,id)){idx=i;break;}
    if(idx==set->user_task_count)return -1;
    WbUserTask *backup=malloc(set->user_task_count*sizeof(*backup));if(!backup)return -1;
    memcpy(backup,set->user_tasks,set->user_task_count*sizeof(*backup));
    for(size_t i=idx+1;i<set->user_task_count;i++){set->user_tasks[i-1]=set->user_tasks[i];rebind_owned_task(&set->user_tasks[i-1]);}
    set->user_task_count--;
    if(wb_command_set_save(store,set)){memcpy(set->user_tasks,backup,(set->user_task_count+1)*sizeof(*backup));set->user_task_count++;for(size_t i=0;i<set->user_task_count;i++)rebind_owned_task(&set->user_tasks[i]);free(backup);return -1;}
    free(backup);return 0;
}

void wb_command_sets_free(CommandSetStore *store){
    if(!store)return;
    for(size_t i=0;i<store->count;i++)wb_command_set_destroy(&store->sets[i]);
    memset(store,0,sizeof(*store));
}
