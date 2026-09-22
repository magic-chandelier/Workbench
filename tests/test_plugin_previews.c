#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "modules/plugins/plugins.h"
#include "modules/extensions/extensions.h"

static void mk(const char *p){assert(mkdir(p,0700)==0);}
static void wr(const char*p,const char*s,mode_t m){int fd=open(p,O_WRONLY|O_CREAT|O_TRUNC,m);assert(fd>=0);size_t n=strlen(s);assert(write(fd,s,n)==(ssize_t)n);assert(close(fd)==0);assert(chmod(p,m)==0);}
static void join(char*out,size_t n,const char*a,const char*b){assert(snprintf(out,n,"%s/%s",a,b)>0);}
int main(void){
 char tmp[]="/tmp/wb-preview-test-XXXXXX";assert(mkdtemp(tmp));char sys[4096],usr[4096],pd[4096],bin[4096],prv[4096],f[4096];join(sys,sizeof(sys),tmp,"sys");join(usr,sizeof(usr),tmp,"usr");mk(sys);mk(usr);join(pd,sizeof(pd),usr,"plugin.archive");mk(pd);join(bin,sizeof(bin),pd,"bin");mk(bin);join(prv,sizeof(prv),pd,"previews");mk(prv);char act[4096];join(act,sizeof(act),pd,"file-actions");mk(act);
 join(f,sizeof(f),pd,"plugin.wbp");wr(f,"WORKBENCH_PLUGIN=1\nformat=1\nid=plugin.archive\nversion=3\napi_min=3\napi_max=3\nname_zh=Archive\nname_en=Archive\ndescription_zh=x\ndescription_en=x\nentry=bin/run\nfile_actions=file-actions\npreview_providers=previews\n",0600);
 join(f,sizeof(f),bin,"run");wr(f,"#!/bin/sh\ncase \"$1\" in\n --workbench-preview-list) printf 'E\\tF\\t5\\t68656c6c6f2e747874\\n' ;;\n --workbench-preview-materialize) printf hello > \"$6\" ;;\n *) exit 2 ;;\nesac\n",0700);
 join(f,sizeof(f),act,"show.wba");wr(f,"WORKBENCH_FILE_ACTION=1\nformat=1\nid=show\nname_zh=显示\nname_en=Show\nentry=bin/run\ntarget=file\nextensions=7z\n",0600);
 join(f,sizeof(f),prv,"archive.wbp");wr(f,"WORKBENCH_FILE_PREVIEW=1\nformat=1\nid=archive\nname_zh=归档\nname_en=Archive\nentry=bin/run\nextensions=7z,zip,tar.gz\n",0600);
 WbPluginRegistry preg;wb_plugin_registry_init(&preg);assert(wb_plugin_registry_scan(&preg,sys,usr)==0);const WbPlugin *pl=wb_plugin_registry_find_active(&preg,"plugin.archive");assert(pl&&wb_plugin_supports_api(pl,3));assert(!strcmp(pl->preview_providers,"previews"));
 WbExtensionRegistry ext;wb_extension_registry_init(&ext);assert(wb_extension_registry_load_plugin_actions(&ext,&preg)==0);assert(wb_extension_preview_count(&ext)==1);assert(wb_extension_registry_count(&ext)==1);const WbRegisteredPreviewProvider *pv=wb_extension_preview_at(&ext,0);assert(pv&&wb_extension_preview_matches_path(pv,"/tmp/a.tar.gz",S_IFREG|0600));char err[256];assert(wb_extension_preview_revalidate(pl,pv,err,sizeof(err))==0);
 char *out=NULL;size_t outn=0;assert(wb_plugin_preview_list(pl,pv->entry,pv->id,"/tmp/a.7z",&out,&outn,err,sizeof(err))==0);assert(outn&&strstr(out,"68656c6c6f2e747874"));free(out);
 char dst[4096];join(dst,sizeof(dst),tmp,"materialized.txt");WbPluginRunResult rr={0};assert(wb_plugin_preview_materialize(pl,pv->entry,pv->id,"/tmp/a.7z","hello.txt",dst,&rr,err,sizeof(err))==0);assert(rr.exited&&rr.exit_code==0);FILE *fp=fopen(dst,"r");assert(fp);char b[16]={0};assert(fread(b,1,5,fp)==5);fclose(fp);assert(!strcmp(b,"hello"));unlink(dst);
 WbRegisteredPreviewProvider saved=*pv;join(f,sizeof(f),act,"show.wba");wr(f,"WORKBENCH_FILE_ACTION=1\nformat=1\nid=show\nname_zh=显示\nname_en=Show\nentry=bin/run\ntarget=file\nextensions=7z\n",0600);
 join(f,sizeof(f),prv,"archive.wbp");wr(f,"WORKBENCH_FILE_PREVIEW=1\nformat=1\nid=archive\nname_zh=篡改\nname_en=Changed\nentry=bin/run\nextensions=7z\n",0600);assert(wb_extension_preview_revalidate(pl,&saved,err,sizeof(err))!=0);
 printf("PLUGIN PREVIEW API PASS\n");return 0;
}
