#define _POSIX_C_SOURCE 200809L
#include "extensions.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
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

static void set_err(char *err,size_t cap,const char *msg){if(err&&cap)snprintf(err,cap,"%s",msg?msg:"");}

static int valid_token(const char *s,size_t max_len){
    if(!s||!*s||strlen(s)>=max_len)return 0;
    for(const unsigned char *p=(const unsigned char*)s;*p;p++){
        if(!(isalnum(*p)||*p=='.'||*p=='_'||*p=='-'))return 0;
    }
    return 1;
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

static int valid_extensions(const char *s){
    if(!s||!*s||strlen(s)>=512)return 0;
    if(!strcmp(s,"*"))return 1;
    const char *p=s;
    while(*p){
        const char *start=p;while(*p&&*p!=',')p++;size_t n=(size_t)(p-start);
        if(!n||n>63)return 0;
        for(size_t i=0;i<n;i++){
            unsigned char c=(unsigned char)start[i];
            if(!(isalnum(c)||c=='.'||c=='_'||c=='+'||c=='-'))return 0;
        }
        if(*p==',')p++;
    }
    return 1;
}

static int parse_target(const char *s,WbFileActionTarget *out){
    if(!strcmp(s,"any"))*out=WB_FILE_ACTION_ANY;
    else if(!strcmp(s,"file"))*out=WB_FILE_ACTION_FILE;
    else if(!strcmp(s,"directory"))*out=WB_FILE_ACTION_DIRECTORY;
    else return -1;
    return 0;
}

void wb_extension_registry_init(WbExtensionRegistry *r){if(r)memset(r,0,sizeof(*r));}
size_t wb_extension_registry_count(const WbExtensionRegistry *r){return r?r->count:0;}

const WbRegisteredExtensionAction *wb_extension_find_action(const WbExtensionRegistry *r,const char *source,const char *id){
    if(!r||!source||!id)return NULL;
    for(size_t i=0;i<r->count;i++){
        const WbRegisteredExtensionAction *a=&r->actions[i];
        if(!strcmp(a->source,source)&&!strcmp(a->id,id))return a;
    }
    return NULL;
}

int wb_extension_register_action(WbExtensionRegistry *r,const WbExtensionActionDescriptor *d){
    if(!r||!d||r->count>=WB_MAX_EXTENSION_ACTIONS)return -1;
    if(!valid_token(d->id,64)||!valid_token(d->source,64)||!d->title_zh||!*d->title_zh||!d->title_en||!*d->title_en||
       !safe_relative_path(d->entry)||!valid_extensions(d->extensions)||d->target<WB_FILE_ACTION_ANY||d->target>WB_FILE_ACTION_DIRECTORY)return -1;
    if(wb_extension_find_action(r,d->source,d->id))return -1;
    WbRegisteredExtensionAction tmp;memset(&tmp,0,sizeof(tmp));
    if(copy_text(tmp.id,sizeof(tmp.id),d->id)||copy_text(tmp.title_zh,sizeof(tmp.title_zh),d->title_zh)||
       copy_text(tmp.title_en,sizeof(tmp.title_en),d->title_en)||copy_text(tmp.source,sizeof(tmp.source),d->source)||
       copy_text(tmp.entry,sizeof(tmp.entry),d->entry)||copy_text(tmp.extensions,sizeof(tmp.extensions),d->extensions))return -1;
    tmp.target=d->target;
    r->actions[r->count++]=tmp;
    return 0;
}

const WbRegisteredExtensionAction *wb_extension_action_at(const WbExtensionRegistry *r,size_t index){return !r||index>=r->count?NULL:&r->actions[index];}

static int open_relative_dir_nofollow(int rootfd,const char *rel){
    if(!safe_relative_path(rel))return -1;
    char buf[512];if(copy_text(buf,sizeof(buf),rel))return -1;
    int cur=dup(rootfd);if(cur<0)return -1;
    char *save=NULL,*part=strtok_r(buf,"/",&save);
    while(part){
        int next=openat(cur,part,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        close(cur);if(next<0)return -1;cur=next;part=strtok_r(NULL,"/",&save);
    }
    return cur;
}

static int open_relative_exec_nofollow(int rootfd,const char *rel){
    if(!safe_relative_path(rel))return -1;
    char buf[512];if(copy_text(buf,sizeof(buf),rel))return -1;
    int cur=dup(rootfd);if(cur<0)return -1;
    char *save=NULL,*part=strtok_r(buf,"/",&save);if(!part){close(cur);return -1;}
    for(;;){
        char *next=strtok_r(NULL,"/",&save);
        if(!next){
            int fd=openat(cur,part,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);close(cur);if(fd<0)return -1;
            struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)||(st.st_mode&0111)==0){close(fd);return -1;}return fd;
        }
        int nfd=openat(cur,part,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);close(cur);if(nfd<0)return -1;cur=nfd;part=next;
    }
}

enum { A_MAGIC=1u<<0,A_FORMAT=1u<<1,A_ID=1u<<2,A_NZH=1u<<3,A_NEN=1u<<4,A_ENTRY=1u<<5,A_TARGET=1u<<6,A_EXT=1u<<7 };
#define A_REQUIRED (A_MAGIC|A_FORMAT|A_ID|A_NZH|A_NEN|A_ENTRY|A_TARGET|A_EXT)
static int once(unsigned *seen,unsigned bit){if(*seen&bit)return -1;*seen|=bit;return 0;}

static int parse_action_fd(int fd,const char *source,WbRegisteredExtensionAction *out){
    FILE *f=fdopen(fd,"r");if(!f){close(fd);return -1;}
    char line[2048];unsigned seen=0;int failed=0;memset(out,0,sizeof(*out));
    if(copy_text(out->source,sizeof(out->source),source)){fclose(f);return -1;}
    while(!failed&&fgets(line,sizeof(line),f)){
        size_t n=strlen(line);if(n==sizeof(line)-1&&line[n-1]!='\n'){failed=1;break;}while(n&&(line[n-1]=='\n'||line[n-1]=='\r'))line[--n]='\0';
        if(!n||line[0]=='#')continue;
        char *eq=strchr(line,'=');
        if(!eq){failed=1;break;}
        *eq++='\0';
        if(!strcmp(line,"WORKBENCH_FILE_ACTION")){if(once(&seen,A_MAGIC)||strcmp(eq,"1"))failed=1;}
        else if(!strcmp(line,"format")){if(once(&seen,A_FORMAT)||strcmp(eq,"1"))failed=1;}
        else if(!strcmp(line,"id")){if(once(&seen,A_ID)||!valid_token(eq,sizeof(out->id))||copy_text(out->id,sizeof(out->id),eq))failed=1;}
        else if(!strcmp(line,"name_zh")){if(once(&seen,A_NZH)||!*eq||copy_text(out->title_zh,sizeof(out->title_zh),eq))failed=1;}
        else if(!strcmp(line,"name_en")){if(once(&seen,A_NEN)||!*eq||copy_text(out->title_en,sizeof(out->title_en),eq))failed=1;}
        else if(!strcmp(line,"entry")){if(once(&seen,A_ENTRY)||!safe_relative_path(eq)||copy_text(out->entry,sizeof(out->entry),eq))failed=1;}
        else if(!strcmp(line,"target")){if(once(&seen,A_TARGET)||parse_target(eq,&out->target))failed=1;}
        else if(!strcmp(line,"extensions")){if(once(&seen,A_EXT)||!valid_extensions(eq)||copy_text(out->extensions,sizeof(out->extensions),eq))failed=1;}
        else failed=1;
    }
    if(ferror(f))failed=1;
    fclose(f);
    return failed||(seen&A_REQUIRED)!=A_REQUIRED?-1:0;
}

static int ends_with(const char *s,const char *suffix){size_t a=strlen(s),b=strlen(suffix);return a>=b&&!strcmp(s+a-b,suffix);}
static int is_wba_name(const char *s){return s&&s[0]!='.'&&ends_with(s,".wba")&&strlen(s)<240;}

static int same_action(const WbRegisteredExtensionAction *a,const WbRegisteredExtensionAction *b){
    return a&&b&&!strcmp(a->id,b->id)&&!strcmp(a->title_zh,b->title_zh)&&!strcmp(a->title_en,b->title_en)&&
           !strcmp(a->source,b->source)&&!strcmp(a->entry,b->entry)&&!strcmp(a->extensions,b->extensions)&&a->target==b->target;
}

static int load_plugin_actions(WbExtensionRegistry *r,const WbPlugin *p){
    if(!p||p->status!=WB_PLUGIN_ACTIVE||!wb_plugin_supports_api(p,2))return 0;
    int rootfd=open(p->root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(rootfd<0)return 0;
    int afd=open_relative_dir_nofollow(rootfd,p->file_actions);if(afd<0){close(rootfd);return 0;}
    DIR *dir=fdopendir(afd);if(!dir){close(afd);close(rootfd);return 0;}
    struct dirent *de;
    while(r->count<WB_MAX_EXTENSION_ACTIONS&&(de=readdir(dir))){
        if(!is_wba_name(de->d_name))continue;
        int fd=openat(dirfd(dir),de->d_name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)continue;
        struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)){close(fd);continue;}
        WbRegisteredExtensionAction a;if(parse_action_fd(fd,p->id,&a))continue;
        int efd=open_relative_exec_nofollow(rootfd,a.entry);if(efd<0)continue;close(efd);
        if(wb_extension_find_action(r,p->id,a.id))continue;
        int n=snprintf(a.descriptor,sizeof(a.descriptor),"%s/%s",p->file_actions,de->d_name);if(n<0||(size_t)n>=sizeof(a.descriptor))continue;
        r->actions[r->count++]=a;
    }
    closedir(dir);close(rootfd);return 0;
}

enum { P_MAGIC=1u<<0,P_FORMAT=1u<<1,P_ID=1u<<2,P_NZH=1u<<3,P_NEN=1u<<4,P_ENTRY=1u<<5,P_EXT=1u<<6 };
#define P_REQUIRED (P_MAGIC|P_FORMAT|P_ID|P_NZH|P_NEN|P_ENTRY|P_EXT)

static int parse_preview_fd(int fd,const char *source,WbRegisteredPreviewProvider *out){
    FILE *f=fdopen(fd,"r");if(!f){close(fd);return -1;}
    char line[2048];unsigned seen=0;int failed=0;memset(out,0,sizeof(*out));
    if(copy_text(out->source,sizeof(out->source),source)){fclose(f);return -1;}
    while(!failed&&fgets(line,sizeof(line),f)){
        size_t n=strlen(line);if(n==sizeof(line)-1&&line[n-1]!='\n'){failed=1;break;}while(n&&(line[n-1]=='\n'||line[n-1]=='\r'))line[--n]='\0';
        if(!n||line[0]=='#')continue;
        char *eq=strchr(line,'=');if(!eq){failed=1;break;}*eq++='\0';
        if(!strcmp(line,"WORKBENCH_FILE_PREVIEW")){if(once(&seen,P_MAGIC)||strcmp(eq,"1"))failed=1;}
        else if(!strcmp(line,"format")){if(once(&seen,P_FORMAT)||strcmp(eq,"1"))failed=1;}
        else if(!strcmp(line,"id")){if(once(&seen,P_ID)||!valid_token(eq,sizeof(out->id))||copy_text(out->id,sizeof(out->id),eq))failed=1;}
        else if(!strcmp(line,"name_zh")){if(once(&seen,P_NZH)||!*eq||copy_text(out->title_zh,sizeof(out->title_zh),eq))failed=1;}
        else if(!strcmp(line,"name_en")){if(once(&seen,P_NEN)||!*eq||copy_text(out->title_en,sizeof(out->title_en),eq))failed=1;}
        else if(!strcmp(line,"entry")){if(once(&seen,P_ENTRY)||!safe_relative_path(eq)||copy_text(out->entry,sizeof(out->entry),eq))failed=1;}
        else if(!strcmp(line,"extensions")){if(once(&seen,P_EXT)||!valid_extensions(eq)||copy_text(out->extensions,sizeof(out->extensions),eq))failed=1;}
        else failed=1;
    }
    if(ferror(f))failed=1;
    fclose(f);return failed||(seen&P_REQUIRED)!=P_REQUIRED?-1:0;
}

static int is_preview_name(const char *s){return s&&s[0]!='.'&&ends_with(s,".wbp")&&strlen(s)<240;}
static int same_preview(const WbRegisteredPreviewProvider *a,const WbRegisteredPreviewProvider *b){
    return a&&b&&!strcmp(a->id,b->id)&&!strcmp(a->title_zh,b->title_zh)&&!strcmp(a->title_en,b->title_en)&&!strcmp(a->source,b->source)&&!strcmp(a->entry,b->entry)&&!strcmp(a->extensions,b->extensions);
}

size_t wb_extension_preview_count(const WbExtensionRegistry *r){return r?r->preview_count:0;}
const WbRegisteredPreviewProvider *wb_extension_preview_at(const WbExtensionRegistry *r,size_t index){return !r||index>=r->preview_count?NULL:&r->previews[index];}
const WbRegisteredPreviewProvider *wb_extension_find_preview(const WbExtensionRegistry *r,const char *source,const char *id){
    if(!r||!source||!id)return NULL;
    for(size_t i=0;i<r->preview_count;i++)if(!strcmp(r->previews[i].source,source)&&!strcmp(r->previews[i].id,id))return &r->previews[i];
    return NULL;
}

static int load_plugin_previews(WbExtensionRegistry *r,const WbPlugin *p){
    if(!p||p->status!=WB_PLUGIN_ACTIVE||!wb_plugin_supports_api(p,3))return 0;
    int rootfd=open(p->root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(rootfd<0)return 0;
    int pfd=open_relative_dir_nofollow(rootfd,p->preview_providers);if(pfd<0){close(rootfd);return 0;}
    DIR *dir=fdopendir(pfd);if(!dir){close(pfd);close(rootfd);return 0;}struct dirent *de;
    while(r->preview_count<WB_MAX_PREVIEW_PROVIDERS&&(de=readdir(dir))){
        if(!is_preview_name(de->d_name))continue;
        int fd=openat(dirfd(dir),de->d_name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)continue;
        struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)){close(fd);continue;}WbRegisteredPreviewProvider pv;if(parse_preview_fd(fd,p->id,&pv))continue;
        int efd=open_relative_exec_nofollow(rootfd,pv.entry);if(efd<0)continue;close(efd);if(wb_extension_find_preview(r,p->id,pv.id))continue;
        int n=snprintf(pv.descriptor,sizeof(pv.descriptor),"%s/%s",p->preview_providers,de->d_name);if(n<0||(size_t)n>=sizeof(pv.descriptor))continue;
        r->previews[r->preview_count++]=pv;
    }
    closedir(dir);close(rootfd);return 0;
}

int wb_extension_registry_load_plugin_actions(WbExtensionRegistry *r,const WbPluginRegistry *plugins){
    if(!r||!plugins)return -1;
    wb_extension_registry_init(r);
    for(size_t i=0;i<plugins->count;i++){load_plugin_actions(r,&plugins->items[i]);load_plugin_previews(r,&plugins->items[i]);}
    return 0;
}

static int ci_equal_n(const char *a,const char *b,size_t n){for(size_t i=0;i<n;i++)if(tolower((unsigned char)a[i])!=tolower((unsigned char)b[i]))return 0;return 1;}
static int extension_filter_matches(const char *filter,const char *path){
    if(!strcmp(filter,"*"))return 1;
    const char *base=strrchr(path,'/');
    base=base?base+1:path;
    size_t blen=strlen(base);
    const char *p=filter;
    while(*p){
        const char *start=p;while(*p&&*p!=',')p++;size_t n=(size_t)(p-start);
        if(blen>n&&base[blen-n-1]=='.'&&ci_equal_n(base+blen-n,start,n))return 1;
        if(*p==',')p++;
    }
    return 0;
}

int wb_extension_action_matches_path(const WbRegisteredExtensionAction *a,const char *path,mode_t mode){
    if(!a||!path||!*path)return 0;
    int is_dir=S_ISDIR(mode),is_file=S_ISREG(mode);
    if(a->target==WB_FILE_ACTION_DIRECTORY&&!is_dir)return 0;
    if(a->target==WB_FILE_ACTION_FILE&&!is_file)return 0;
    if(a->target==WB_FILE_ACTION_ANY&&!is_dir&&!is_file)return 0;
    if(is_dir)return a->target!=WB_FILE_ACTION_FILE;
    return extension_filter_matches(a->extensions,path);
}

int wb_extension_action_revalidate(const WbPlugin *plugin,const WbRegisteredExtensionAction *a,char *err,size_t err_cap){
    if(!plugin||!a||strcmp(plugin->id,a->source)||plugin->status!=WB_PLUGIN_ACTIVE||!wb_plugin_supports_api(plugin,2)){set_err(err,err_cap,"file action plugin unavailable");return -1;}
    int rootfd=open(plugin->root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(rootfd<0){set_err(err,err_cap,"plugin directory unavailable");return -1;}
    if(!safe_relative_path(a->descriptor)){close(rootfd);set_err(err,err_cap,"invalid file action descriptor path");return -1;}
    char buf[512];if(copy_text(buf,sizeof(buf),a->descriptor)){close(rootfd);set_err(err,err_cap,"file action descriptor path too long");return -1;}
    char *slash=strrchr(buf,'/');if(!slash){close(rootfd);set_err(err,err_cap,"invalid file action descriptor path");return -1;}*slash++='\0';
    int dfd=open_relative_dir_nofollow(rootfd,buf);if(dfd<0){close(rootfd);set_err(err,err_cap,"file action directory unavailable");return -1;}
    int fd=openat(dfd,slash,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);close(dfd);if(fd<0){close(rootfd);set_err(err,err_cap,"file action descriptor unavailable");return -1;}
    struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)){close(fd);close(rootfd);set_err(err,err_cap,"invalid file action descriptor");return -1;}
    WbRegisteredExtensionAction fresh;if(parse_action_fd(fd,plugin->id,&fresh)||!same_action(a,&fresh)){close(rootfd);set_err(err,err_cap,"file action descriptor changed or invalid");return -1;}
    int efd=open_relative_exec_nofollow(rootfd,fresh.entry);close(rootfd);if(efd<0){set_err(err,err_cap,"file action entry unavailable");return -1;}close(efd);set_err(err,err_cap,"");return 0;
}


int wb_extension_preview_matches_path(const WbRegisteredPreviewProvider *p,const char *path,mode_t mode){
    if(!p||!path||!*path||!S_ISREG(mode))return 0;
    return extension_filter_matches(p->extensions,path);
}

int wb_extension_preview_revalidate(const WbPlugin *plugin,const WbRegisteredPreviewProvider *p,char *err,size_t err_cap){
    if(!plugin||!p||strcmp(plugin->id,p->source)||plugin->status!=WB_PLUGIN_ACTIVE||!wb_plugin_supports_api(plugin,3)){set_err(err,err_cap,"preview plugin unavailable");return -1;}
    int rootfd=open(plugin->root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(rootfd<0){set_err(err,err_cap,"plugin directory unavailable");return -1;}
    if(!safe_relative_path(p->descriptor)){close(rootfd);set_err(err,err_cap,"invalid preview descriptor path");return -1;}
    char buf[512];if(copy_text(buf,sizeof(buf),p->descriptor)){close(rootfd);set_err(err,err_cap,"preview descriptor path too long");return -1;}
    char *slash=strrchr(buf,'/');if(!slash){close(rootfd);set_err(err,err_cap,"invalid preview descriptor path");return -1;}*slash++='\0';
    int dfd=open_relative_dir_nofollow(rootfd,buf);if(dfd<0){close(rootfd);set_err(err,err_cap,"preview directory unavailable");return -1;}
    int fd=openat(dfd,slash,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);close(dfd);if(fd<0){close(rootfd);set_err(err,err_cap,"preview descriptor unavailable");return -1;}
    struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)){close(fd);close(rootfd);set_err(err,err_cap,"invalid preview descriptor");return -1;}
    WbRegisteredPreviewProvider fresh;if(parse_preview_fd(fd,plugin->id,&fresh)||!same_preview(p,&fresh)){close(rootfd);set_err(err,err_cap,"preview descriptor changed or invalid");return -1;}
    int efd=open_relative_exec_nofollow(rootfd,fresh.entry);close(rootfd);if(efd<0){set_err(err,err_cap,"preview entry unavailable");return -1;}close(efd);set_err(err,err_cap,"");return 0;
}
