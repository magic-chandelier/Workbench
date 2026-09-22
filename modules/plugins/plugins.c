#define _POSIX_C_SOURCE 200809L
#include "plugins.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

static int copy_text(char *dst,size_t cap,const char *src){
    if(!dst||!cap||!src)return -1;
    size_t n=strlen(src);
    if(n>=cap)return -1;
    memcpy(dst,src,n+1);
    return 0;
}

int wb_plugins_valid_id(const char *id){
    if(!id||!*id||strlen(id)>=64||!strcmp(id,".")||!strcmp(id,".."))return 0;
    for(const unsigned char *p=(const unsigned char*)id;*p;p++)if(!(isalnum(*p)||*p=='.'||*p=='_'||*p=='-'))return 0;
    return 1;
}

int wb_plugin_api_compatible(int api_min,int api_max){
    if(api_min<1||api_max<api_min)return 0;
    return api_min<=WB_PLUGIN_API_VERSION&&api_max>=WB_PLUGIN_API_MIN_SUPPORTED;
}

int wb_plugin_supports_api(const WbPlugin *plugin,int api_version){
    return plugin&&api_version>=WB_PLUGIN_API_MIN_SUPPORTED&&api_version<=WB_PLUGIN_API_VERSION&&
           wb_plugin_api_compatible(plugin->api_min,plugin->api_max)&&plugin->api_max>=api_version;
}

static int safe_relative_path(const char *s){
    if(!s||!*s||*s=='/'||strlen(s)>=512)return 0;
    const char *p=s;
    while(*p){
        const char *start=p;while(*p&&*p!='/')p++;size_t n=(size_t)(p-start);
        if(!n||(n==1&&start[0]=='.')||(n==2&&start[0]=='.'&&start[1]=='.'))return 0;
        for(size_t i=0;i<n;i++)if((unsigned char)start[i]<0x20||(unsigned char)start[i]==0x7f)return 0;
        if(*p=='/')p++;
    }
    return 1;
}

int wb_plugins_default_roots(char *system_root,size_t system_cap,char *user_root,size_t user_cap){
    const char *home=getenv("HOME");if(!system_root||!user_root||!home||!*home)return -1;
    if(copy_text(system_root,system_cap,"/usr/local/share/workbench/plugins"))return -1;
    int n=snprintf(user_root,user_cap,"%s/.local/share/workbench/plugins",home);return n<0||(size_t)n>=user_cap?-1:0;
}

void wb_plugin_registry_init(WbPluginRegistry *r){if(r)memset(r,0,sizeof(*r));}
void wb_plugin_registry_clear(WbPluginRegistry *r){if(r)memset(r,0,sizeof(*r));}

static void set_error(WbPlugin *p,WbPluginStatus status,const char *msg){p->status=status;snprintf(p->error,sizeof(p->error),"%s",msg?msg:"");}

static int parse_int(const char *s,int lo,int hi,int *out){char *e=NULL;long v;if(!s||!*s)return -1;errno=0;v=strtol(s,&e,10);if(errno||!e||*e||v<lo||v>hi)return -1;*out=(int)v;return 0;}

enum {F_MAGIC=1u<<0,F_FORMAT=1u<<1,F_ID=1u<<2,F_VERSION=1u<<3,F_MIN=1u<<4,F_MAX=1u<<5,F_NZH=1u<<6,F_NEN=1u<<7,F_DZH=1u<<8,F_DEN=1u<<9};
#define REQUIRED_FIELDS (F_MAGIC|F_FORMAT|F_ID|F_VERSION|F_MIN|F_MAX|F_NZH|F_NEN|F_DZH|F_DEN)

static int set_once(unsigned *seen,unsigned bit){if(*seen&bit)return -1;*seen|=bit;return 0;}

static int parse_manifest_fd(int fd,const char *dirname,WbPlugin *out){
    FILE *f=fdopen(fd,"r");if(!f){close(fd);return -1;}char line[2048];unsigned seen=0;int failed=0;
    out->format=0;out->api_min=0;out->api_max=0;snprintf(out->command_sets,sizeof(out->command_sets),"command-sets");snprintf(out->file_actions,sizeof(out->file_actions),"file-actions");snprintf(out->preview_providers,sizeof(out->preview_providers),"preview-providers");
    while(!failed&&fgets(line,sizeof(line),f)){
        size_t n=strlen(line);if(n==sizeof(line)-1&&line[n-1]!='\n'){failed=1;break;}while(n&&(line[n-1]=='\n'||line[n-1]=='\r'))line[--n]='\0';
        if(!n||line[0]=='#')continue;
        char *eq=strchr(line,'=');
        if(!eq){failed=1;break;}
        *eq++='\0';
        if(!strcmp(line,"WORKBENCH_PLUGIN")){if(set_once(&seen,F_MAGIC)||strcmp(eq,"1"))failed=1;}
        else if(!strcmp(line,"format")){if(set_once(&seen,F_FORMAT)||parse_int(eq,1,1,&out->format))failed=1;}
        else if(!strcmp(line,"id")){if(set_once(&seen,F_ID)||!wb_plugins_valid_id(eq)||copy_text(out->id,sizeof(out->id),eq))failed=1;}
        else if(!strcmp(line,"version")){if(set_once(&seen,F_VERSION)||!*eq||copy_text(out->version,sizeof(out->version),eq))failed=1;}
        else if(!strcmp(line,"api_min")){if(set_once(&seen,F_MIN)||parse_int(eq,1,999,&out->api_min))failed=1;}
        else if(!strcmp(line,"api_max")){if(set_once(&seen,F_MAX)||parse_int(eq,1,999,&out->api_max))failed=1;}
        else if(!strcmp(line,"name_zh")){if(set_once(&seen,F_NZH)||!*eq||copy_text(out->name_zh,sizeof(out->name_zh),eq))failed=1;}
        else if(!strcmp(line,"name_en")){if(set_once(&seen,F_NEN)||!*eq||copy_text(out->name_en,sizeof(out->name_en),eq))failed=1;}
        else if(!strcmp(line,"description_zh")){if(set_once(&seen,F_DZH)||copy_text(out->description_zh,sizeof(out->description_zh),eq))failed=1;}
        else if(!strcmp(line,"description_en")){if(set_once(&seen,F_DEN)||copy_text(out->description_en,sizeof(out->description_en),eq))failed=1;}
        else if(!strcmp(line,"entry")){if(out->entry[0]||!safe_relative_path(eq)||copy_text(out->entry,sizeof(out->entry),eq))failed=1;}
        else if(!strcmp(line,"command_sets")){if(!safe_relative_path(eq)||copy_text(out->command_sets,sizeof(out->command_sets),eq))failed=1;}
        else if(!strcmp(line,"file_actions")){if(!safe_relative_path(eq)||copy_text(out->file_actions,sizeof(out->file_actions),eq))failed=1;}
        else if(!strcmp(line,"preview_providers")){if(!safe_relative_path(eq)||copy_text(out->preview_providers,sizeof(out->preview_providers),eq))failed=1;}
    }
    if(ferror(f))failed=1;
    fclose(f);
    if(failed||(seen&REQUIRED_FIELDS)!=REQUIRED_FIELDS||strcmp(out->id,dirname)||out->api_min>out->api_max)return -1;
    return 0;
}

static ssize_t active_index(const WbPluginRegistry *r,const char *id){
    for(size_t i=0;i<r->count;i++){
        if(r->items[i].status==WB_PLUGIN_ACTIVE&&!strcmp(r->items[i].id,id))return (ssize_t)i;
    }
    return -1;
}

static void add_candidate(WbPluginRegistry *r,int rootfd,const char *root,const char *name,WbPluginSource source){
    if(r->count>=WB_MAX_PLUGINS||!wb_plugins_valid_id(name))return;
    struct stat st;if(fstatat(rootfd,name,&st,AT_SYMLINK_NOFOLLOW)||!S_ISDIR(st.st_mode))return;
    if(!strncmp(name,".wb-removing-",13))return;
    WbPlugin p;memset(&p,0,sizeof(p));p.source=source;p.enabled=1;p.status=WB_PLUGIN_INVALID;snprintf(p.id,sizeof(p.id),"%s",name);
    if(snprintf(p.root,sizeof(p.root),"%s/%s",root,name)<0){return;}
    int dfd=openat(rootfd,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(dfd<0){set_error(&p,WB_PLUGIN_INVALID,"cannot open plugin directory");r->items[r->count++]=p;return;}
    int mfd=openat(dfd,"plugin.wbp",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(mfd<0){close(dfd);set_error(&p,WB_PLUGIN_INVALID,"missing plugin.wbp");r->items[r->count++]=p;return;}
    if(parse_manifest_fd(mfd,name,&p)){close(dfd);set_error(&p,WB_PLUGIN_INVALID,"invalid plugin.wbp");r->items[r->count++]=p;return;}
    int disabled=0;struct stat dst;
    if(fstatat(dfd,".workbench-disabled",&dst,AT_SYMLINK_NOFOLLOW)==0){
        if(!S_ISREG(dst.st_mode)){close(dfd);set_error(&p,WB_PLUGIN_INVALID,"invalid disable marker");r->items[r->count++]=p;return;}
        disabled=1;
    }else if(errno!=ENOENT){close(dfd);set_error(&p,WB_PLUGIN_INVALID,"cannot inspect disable marker");r->items[r->count++]=p;return;}
    close(dfd);
    if(!wb_plugin_api_compatible(p.api_min,p.api_max)){set_error(&p,WB_PLUGIN_INCOMPATIBLE,"plugin API incompatible");r->items[r->count++]=p;return;}
    ssize_t prior=active_index(r,p.id);
    if(prior>=0){
        if(source==WB_PLUGIN_USER&&r->items[prior].source==WB_PLUGIN_SYSTEM){
            r->items[prior].status=WB_PLUGIN_SHADOWED;
            p.status=disabled?WB_PLUGIN_DISABLED:WB_PLUGIN_ACTIVE;
            p.enabled=disabled?0:1;
        }else p.status=WB_PLUGIN_SHADOWED;
    }else{
        p.status=disabled?WB_PLUGIN_DISABLED:WB_PLUGIN_ACTIVE;
        p.enabled=disabled?0:1;
    }
    r->items[r->count++]=p;
}

static void scan_root(WbPluginRegistry *r,const char *root,WbPluginSource source){
    int fd=open(root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return;DIR *d=fdopendir(fd);if(!d){close(fd);return;}struct dirent *de;
    while((de=readdir(d))){if(de->d_name[0]=='.')continue;add_candidate(r,dirfd(d),root,de->d_name,source);}closedir(d);
}

static int plugin_compare(const void *a,const void *b){
    const WbPlugin *pa=(const WbPlugin*)a,*pb=(const WbPlugin*)b;
    int c=strcmp(pa->id,pb->id);if(c)return c;
    if(pa->source!=pb->source)return pa->source<pb->source?-1:1;
    if(pa->status!=pb->status)return pa->status<pb->status?-1:1;
    return strcmp(pa->version,pb->version);
}

int wb_plugin_registry_scan(WbPluginRegistry *r,const char *system_root,const char *user_root){
    if(!r||!system_root||!user_root)return -1;
    memset(r,0,sizeof(*r));
    if(copy_text(r->system_root,sizeof(r->system_root),system_root)||copy_text(r->user_root,sizeof(r->user_root),user_root))return -1;
    scan_root(r,system_root,WB_PLUGIN_SYSTEM);
    scan_root(r,user_root,WB_PLUGIN_USER);
    if(r->count>1)qsort(r->items,r->count,sizeof(r->items[0]),plugin_compare);
    return 0;
}

size_t wb_plugin_registry_active_count(const WbPluginRegistry *r){size_t n=0;if(r)for(size_t i=0;i<r->count;i++)if(r->items[i].status==WB_PLUGIN_ACTIVE)n++;return n;}
const WbPlugin *wb_plugin_registry_active_at(const WbPluginRegistry *r,size_t index){if(!r)return NULL;for(size_t i=0;i<r->count;i++)if(r->items[i].status==WB_PLUGIN_ACTIVE){if(index==0)return &r->items[i];index--;}return NULL;}
const WbPlugin *wb_plugin_registry_find_active(const WbPluginRegistry *r,const char *id){if(!r||!id)return NULL;for(size_t i=0;i<r->count;i++)if(r->items[i].status==WB_PLUGIN_ACTIVE&&!strcmp(r->items[i].id,id))return &r->items[i];return NULL;}

extern char **environ;

static void error_text(char *err,size_t cap,const char *msg){
    if(err&&cap)snprintf(err,cap,"%s",msg?msg:"");
}

static int open_relative_file_nofollow(int rootfd,const char *rel,struct stat *out_st){
    if(!safe_relative_path(rel))return -1;
    char buf[512];
    if(copy_text(buf,sizeof(buf),rel))return -1;
    int cur=dup(rootfd);
    if(cur<0)return -1;
    char *save=NULL;
    char *part=strtok_r(buf,"/",&save);
    if(!part){close(cur);return -1;}
    for(;;){
        char *next=strtok_r(NULL,"/",&save);
        if(!next){
            int fd=openat(cur,part,O_RDONLY|O_NOFOLLOW);
            close(cur);
            if(fd<0)return -1;
            struct stat st;
            if(fstat(fd,&st)||!S_ISREG(st.st_mode)||(st.st_mode&0111)==0){close(fd);return -1;}
            if(out_st)*out_st=st;
            return fd;
        }
        int nfd=openat(cur,part,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        close(cur);
        if(nfd<0)return -1;
        cur=nfd;
        part=next;
    }
}

static int revalidate_plugin(const WbPlugin *plugin,int *dirfd_out,WbPlugin *fresh,char *err,size_t err_cap){
    if(!plugin||!wb_plugins_valid_id(plugin->id)){error_text(err,err_cap,"invalid plugin id");return -1;}
    int dfd=open(plugin->root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(dfd<0){error_text(err,err_cap,"plugin directory unavailable");return -1;}
    struct stat disabled_st;
    if(fstatat(dfd,".workbench-disabled",&disabled_st,AT_SYMLINK_NOFOLLOW)==0){
        close(dfd);error_text(err,err_cap,S_ISREG(disabled_st.st_mode)?"plugin disabled":"invalid disable marker");return -1;
    }else if(errno!=ENOENT){close(dfd);error_text(err,err_cap,"cannot inspect disable marker");return -1;}
    int mfd=openat(dfd,"plugin.wbp",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(mfd<0){close(dfd);error_text(err,err_cap,"plugin manifest unavailable");return -1;}
    memset(fresh,0,sizeof(*fresh));
    fresh->source=plugin->source;
    fresh->enabled=1;
    if(parse_manifest_fd(mfd,plugin->id,fresh)){
        close(dfd);error_text(err,err_cap,"plugin manifest changed or invalid");return -1;
    }
    if(!wb_plugin_api_compatible(fresh->api_min,fresh->api_max)){
        close(dfd);error_text(err,err_cap,"plugin API incompatible");return -1;
    }
    if(copy_text(fresh->root,sizeof(fresh->root),plugin->root)){
        close(dfd);error_text(err,err_cap,"plugin path too long");return -1;
    }
    *dirfd_out=dfd;
    return 0;
}

static int tty_set_foreground_pgrp(int fd,pid_t pgrp){
    sigset_t set,old;
    sigemptyset(&set);sigaddset(&set,SIGTTOU);
    if(sigprocmask(SIG_BLOCK,&set,&old))return -1;
    int rc=tcsetpgrp(fd,pgrp);
    int saved=errno;
    (void)sigprocmask(SIG_SETMASK,&old,NULL);
    errno=saved;
    return rc;
}

static int run_open_entry_mode(int efd,char *const argv[],int interactive,WbPluginRunResult *result,char *err,size_t err_cap){
    if(result)memset(result,0,sizeof(*result));
    int gate[2]={-1,-1};
    int use_tty=interactive&&isatty(STDIN_FILENO);
    pid_t parent_pgrp=getpgrp();
    if(use_tty&&pipe(gate)){close(efd);error_text(err,err_cap,"plugin launch synchronization failed");return -1;}
    pid_t pid=fork();
    if(pid<0){if(gate[0]>=0){close(gate[0]);close(gate[1]);}close(efd);error_text(err,err_cap,"fork failed");return -1;}
    if(pid==0){
        if(use_tty){
            close(gate[1]);
            (void)setpgid(0,0);
            char token=0;ssize_t n;
            do{n=read(gate[0],&token,1);}while(n<0&&errno==EINTR);
            close(gate[0]);
            if(n!=1)_exit(125);
        }else (void)setpgid(0,0);
        fexecve(efd,argv,environ);
        _exit(126);
    }
    close(efd);
    if(use_tty)close(gate[0]);
    (void)setpgid(pid,pid);
    if(use_tty){
        if(tty_set_foreground_pgrp(STDIN_FILENO,pid)){
            close(gate[1]);
            (void)kill(-pid,SIGKILL);(void)waitpid(pid,NULL,0);
            (void)tty_set_foreground_pgrp(STDIN_FILENO,parent_pgrp);
            error_text(err,err_cap,"cannot give terminal control to plugin");return -1;
        }
        char token='1';ssize_t n;
        do{n=write(gate[1],&token,1);}while(n<0&&errno==EINTR);
        close(gate[1]);
        if(n!=1){
            (void)kill(-pid,SIGKILL);(void)waitpid(pid,NULL,0);
            (void)tty_set_foreground_pgrp(STDIN_FILENO,parent_pgrp);
            error_text(err,err_cap,"cannot start interactive plugin");return -1;
        }
    }
    int st=0,wait_failed=0;
    while(waitpid(pid,&st,0)<0){if(errno==EINTR)continue;wait_failed=1;break;}
    if(use_tty)(void)tty_set_foreground_pgrp(STDIN_FILENO,parent_pgrp);
    if(wait_failed){error_text(err,err_cap,"waitpid failed");return -1;}
    if(kill(-pid,SIGTERM)==0){
        struct timespec ts={0,50000000L};
        nanosleep(&ts,NULL);
        (void)kill(-pid,SIGKILL);
    }
    if(result){
        if(WIFEXITED(st)){result->exited=1;result->exit_code=WEXITSTATUS(st);}
        if(WIFSIGNALED(st)){result->signaled=1;result->term_signal=WTERMSIG(st);}
    }
    error_text(err,err_cap,"");
    return 0;
}

static int run_open_entry(int efd,char *const argv[],WbPluginRunResult *result,char *err,size_t err_cap){
    return run_open_entry_mode(efd,argv,0,result,err,err_cap);
}

static int run_open_entry_interactive(int efd,char *const argv[],WbPluginRunResult *result,char *err,size_t err_cap){
    return run_open_entry_mode(efd,argv,1,result,err,err_cap);
}

int wb_plugin_launch(const WbPlugin *plugin,WbPluginRunResult *result,char *err,size_t err_cap){
    WbPlugin fresh;int dfd=-1;
    if(revalidate_plugin(plugin,&dfd,&fresh,err,err_cap))return -1;
    if(!fresh.entry[0]){close(dfd);error_text(err,err_cap,"plugin has no application entry");return -1;}
    int efd=open_relative_file_nofollow(dfd,fresh.entry,NULL);close(dfd);
    if(efd<0){error_text(err,err_cap,"plugin entry is missing, unsafe, or not executable");return -1;}
    char *const argv[]={fresh.entry,NULL};
    return run_open_entry_interactive(efd,argv,result,err,err_cap);
}

int wb_plugin_launch_file_action(const WbPlugin *plugin,const char *entry,const char *action_id,const char *selected_path,WbPluginRunResult *result,char *err,size_t err_cap){
    if(!plugin||!entry||!safe_relative_path(entry)||!action_id||!wb_plugins_valid_id(action_id)||!selected_path||!*selected_path){error_text(err,err_cap,"invalid file action launch request");return -1;}
    if(selected_path[0]!='/'){error_text(err,err_cap,"selected path must be absolute");return -1;}
    WbPlugin fresh;int dfd=-1;
    if(revalidate_plugin(plugin,&dfd,&fresh,err,err_cap))return -1;
    if(!wb_plugin_supports_api(&fresh,2)){close(dfd);error_text(err,err_cap,"plugin does not support required API");return -1;}
    int efd=open_relative_file_nofollow(dfd,entry,NULL);close(dfd);
    if(efd<0){error_text(err,err_cap,"plugin entry is missing, unsafe, or not executable");return -1;}
    char *const argv[]={(char*)entry,"--workbench-file-action",(char*)action_id,"--",(char*)selected_path,NULL};
    return run_open_entry_interactive(efd,argv,result,err,err_cap);
}


static int run_open_entry_capture(int efd,char *const argv[],char **output,size_t *output_len,char *err,size_t err_cap){
    if(output)*output=NULL;
    if(output_len)*output_len=0;
    if(!output||!output_len){close(efd);error_text(err,err_cap,"invalid capture request");return -1;}
    int pipefd[2];if(pipe(pipefd)){close(efd);error_text(err,err_cap,"pipe failed");return -1;}
    pid_t pid=fork();
    if(pid<0){close(efd);close(pipefd[0]);close(pipefd[1]);error_text(err,err_cap,"fork failed");return -1;}
    if(pid==0){
        (void)setpgid(0,0);close(pipefd[0]);
        if(dup2(pipefd[1],STDOUT_FILENO)<0)_exit(126);
        close(pipefd[1]);
        fexecve(efd,argv,environ);_exit(126);
    }
    close(efd);close(pipefd[1]);(void)setpgid(pid,pid);
    size_t cap=65536,used=0;char *buf=malloc(cap+1);int failed=buf?0:1;
    while(!failed){
        if(used==cap){if(cap>=16u*1024u*1024u){failed=1;break;}size_t nc=cap*2;if(nc>16u*1024u*1024u)nc=16u*1024u*1024u;char *nb=realloc(buf,nc+1);if(!nb){failed=1;break;}buf=nb;cap=nc;}
        ssize_t n=read(pipefd[0],buf+used,cap-used);if(n==0)break;if(n<0){if(errno==EINTR)continue;failed=1;break;}used+=(size_t)n;
    }
    close(pipefd[0]);if(failed)(void)kill(-pid,SIGKILL);
    int st=0;while(waitpid(pid,&st,0)<0){if(errno==EINTR)continue;free(buf);error_text(err,err_cap,"waitpid failed");return -1;}
    if(kill(-pid,SIGTERM)==0){struct timespec ts={0,50000000L};nanosleep(&ts,NULL);(void)kill(-pid,SIGKILL);}
    if(failed||!WIFEXITED(st)||WEXITSTATUS(st)!=0){free(buf);error_text(err,err_cap,failed?"preview output too large or unreadable":"preview provider failed");return -1;}
    buf[used]='\0';*output=buf;*output_len=used;error_text(err,err_cap,"");return 0;
}

int wb_plugin_preview_list(const WbPlugin *plugin,const char *entry,const char *provider_id,const char *archive,char **output,size_t *output_len,char *err,size_t err_cap){
    if(!plugin||!entry||!safe_relative_path(entry)||!provider_id||!wb_plugins_valid_id(provider_id)||!archive||archive[0]!='/'){error_text(err,err_cap,"invalid preview list request");return -1;}
    WbPlugin fresh;int dfd=-1;if(revalidate_plugin(plugin,&dfd,&fresh,err,err_cap))return -1;
    if(!wb_plugin_supports_api(&fresh,3)){close(dfd);error_text(err,err_cap,"plugin does not support preview API");return -1;}
    int efd=open_relative_file_nofollow(dfd,entry,NULL);close(dfd);if(efd<0){error_text(err,err_cap,"preview entry unavailable");return -1;}
    char *const argv[]={(char*)entry,"--workbench-preview-list",(char*)provider_id,"--",(char*)archive,NULL};
    return run_open_entry_capture(efd,argv,output,output_len,err,err_cap);
}

int wb_plugin_preview_materialize(const WbPlugin *plugin,const char *entry,const char *provider_id,const char *archive,const char *member,const char *destination,WbPluginRunResult *result,char *err,size_t err_cap){
    if(!plugin||!entry||!safe_relative_path(entry)||!provider_id||!wb_plugins_valid_id(provider_id)||!archive||archive[0]!='/'||!member||!*member||!destination||destination[0]!='/'){error_text(err,err_cap,"invalid preview materialize request");return -1;}
    WbPlugin fresh;int dfd=-1;if(revalidate_plugin(plugin,&dfd,&fresh,err,err_cap))return -1;
    if(!wb_plugin_supports_api(&fresh,3)){close(dfd);error_text(err,err_cap,"plugin does not support preview API");return -1;}
    int efd=open_relative_file_nofollow(dfd,entry,NULL);close(dfd);if(efd<0){error_text(err,err_cap,"preview entry unavailable");return -1;}
    char *const argv[]={(char*)entry,"--workbench-preview-materialize",(char*)provider_id,"--",(char*)archive,(char*)member,(char*)destination,NULL};
    return run_open_entry(efd,argv,result,err,err_cap);
}

static int remove_tree_at(int parentfd,const char *name){
    struct stat st;
    if(fstatat(parentfd,name,&st,AT_SYMLINK_NOFOLLOW))return errno==ENOENT?0:-1;
    if(!S_ISDIR(st.st_mode))return unlinkat(parentfd,name,0);
    int fd=openat(parentfd,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0)return -1;
    DIR *dir=fdopendir(fd);
    if(!dir){close(fd);return -1;}
    struct dirent *de;int rc=0;
    while((de=readdir(dir))){
        if(!strcmp(de->d_name,".")||!strcmp(de->d_name,".."))continue;
        if(remove_tree_at(dirfd(dir),de->d_name)){rc=-1;break;}
    }
    if(closedir(dir)&&!rc)rc=-1;
    if(!rc&&unlinkat(parentfd,name,AT_REMOVEDIR))rc=-1;
    return rc;
}

static int remove_plugin_from_root(const char *root,const char *id,char *err,size_t err_cap){
    if(!root||!wb_plugins_valid_id(id)){error_text(err,err_cap,"invalid plugin id");return -1;}
    int rootfd=open(root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(rootfd<0){error_text(err,err_cap,"plugin root unavailable");return -1;}
    struct stat st;
    if(fstatat(rootfd,id,&st,AT_SYMLINK_NOFOLLOW)||!S_ISDIR(st.st_mode)){
        close(rootfd);error_text(err,err_cap,"plugin directory unavailable");return -1;
    }
    char tmp[128];
    struct timespec now={0,0};
    (void)clock_gettime(CLOCK_MONOTONIC,&now);
    int n=snprintf(tmp,sizeof(tmp),".wb-removing-%s-%ld-%ld",id,(long)getpid(),(long)now.tv_nsec);
    if(n<0||(size_t)n>=sizeof(tmp)){close(rootfd);error_text(err,err_cap,"temporary name too long");return -1;}
    if(fstatat(rootfd,tmp,&st,AT_SYMLINK_NOFOLLOW)==0||errno!=ENOENT){close(rootfd);error_text(err,err_cap,"temporary removal path exists");return -1;}
    if(renameat(rootfd,id,rootfd,tmp)){close(rootfd);error_text(err,err_cap,"cannot atomically detach plugin");return -1;}
    int rc=remove_tree_at(rootfd,tmp);
    close(rootfd);
    if(rc){error_text(err,err_cap,"plugin detached but cleanup failed");return -1;}
    error_text(err,err_cap,"");
    return 0;
}

int wb_plugin_uninstall(const WbPluginRegistry *registry,const WbPlugin *plugin,char *err,size_t err_cap){
    if(!registry||!plugin){error_text(err,err_cap,"invalid uninstall request");return -1;}
    const char *root=plugin->source==WB_PLUGIN_USER?registry->user_root:registry->system_root;
    char expected[WB_PLUGIN_PATH_MAX];
    int n=snprintf(expected,sizeof(expected),"%s/%s",root,plugin->id);
    if(n<0||(size_t)n>=sizeof(expected)||strcmp(expected,plugin->root)){
        error_text(err,err_cap,"plugin path does not match registry root");return -1;
    }
    return remove_plugin_from_root(root,plugin->id,err,err_cap);
}

int wb_plugin_remove_system_by_id(const char *system_root,const char *id,char *err,size_t err_cap){
    return remove_plugin_from_root(system_root,id,err,err_cap);
}

static int registry_plugin_root_matches(const WbPluginRegistry *registry,const WbPlugin *plugin,const char **root_out){
    if(!registry||!plugin||!wb_plugins_valid_id(plugin->id))return 0;
    const char *root=plugin->source==WB_PLUGIN_USER?registry->user_root:registry->system_root;
    if(!root||!*root)return 0;
    char expected[WB_PLUGIN_PATH_MAX];
    int n=snprintf(expected,sizeof(expected),"%s/%s",root,plugin->id);
    if(n<0||(size_t)n>=sizeof(expected)||strcmp(expected,plugin->root))return 0;
    if(root_out)*root_out=root;
    return 1;
}

int wb_plugin_set_enabled(const WbPluginRegistry *registry,const WbPlugin *plugin,int enabled,char *err,size_t err_cap){
    const char *root=NULL;
    if(!registry_plugin_root_matches(registry,plugin,&root)){error_text(err,err_cap,"plugin path does not match registry root");return -1;}
    int rootfd=open(root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(rootfd<0){error_text(err,err_cap,"plugin root unavailable");return -1;}
    int dfd=openat(rootfd,plugin->id,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    close(rootfd);
    if(dfd<0){error_text(err,err_cap,"plugin directory unavailable");return -1;}
    int rc=0;
    if(enabled){
        if(unlinkat(dfd,".workbench-disabled",0)&&errno!=ENOENT){error_text(err,err_cap,"cannot enable plugin");rc=-1;}
    }else{
        int fd=openat(dfd,".workbench-disabled",O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if(fd>=0)close(fd);
        else if(errno==EEXIST){
            struct stat st;
            if(fstatat(dfd,".workbench-disabled",&st,AT_SYMLINK_NOFOLLOW)||!S_ISREG(st.st_mode)){error_text(err,err_cap,"invalid disable marker");rc=-1;}
        }else{error_text(err,err_cap,"cannot disable plugin");rc=-1;}
    }
    close(dfd);
    if(!rc)error_text(err,err_cap,"");
    return rc;
}
