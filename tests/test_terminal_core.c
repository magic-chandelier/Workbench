#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "modules/terminal/terminal.h"

static void die(const char *m){fprintf(stderr,"FAIL: %s\n",m);exit(1);}
static int contains(WbTerminalSession *s,const char *needle){
    for(size_t i=0;i<wb_terminal_line_count(s);i++){
        const char *line=wb_terminal_line_at(s,i);
        if(line&&strstr(line,needle))return 1;
    }
    const char *cur=wb_terminal_current_line(s);
    return cur&&strstr(cur,needle);
}
int main(void){
    WbTerminalSession s;
    if(wb_terminal_init(&s,3)!=0)die("init");
    const unsigned char basic[]="one\ntwo\nthree\nfour\n";
    wb_terminal_feed(&s,basic,sizeof(basic)-1);
    if(wb_terminal_line_count(&s)!=3)die("line limit not enforced");
    if(strcmp(wb_terminal_line_at(&s,0),"two")||strcmp(wb_terminal_line_at(&s,2),"four"))die("oldest line not dropped");
    wb_terminal_clear(&s);
    const unsigned char ansi[]="\033[31mred\033[0m\rRED\nabc\bD\n\033[2Jafter";
    wb_terminal_feed(&s,ansi,sizeof(ansi)-1);
    if(wb_terminal_line_count(&s)!=2)die("CSI 2J must preserve short-lived history");
    if(wb_terminal_screen_floor(&s)!=2)die("CSI 2J must advance visible screen floor");
    if(strcmp(wb_terminal_current_line(&s),"after"))die("parser current line after clear");
    wb_terminal_destroy(&s);

    if(wb_terminal_init(&s,20)!=0)die("pty init");
    char err[256]={0};
    if(wb_terminal_start(&s,"/bin/sh",80,20,err,sizeof(err))!=0){fprintf(stderr,"PTY start: %s\n",err);return 1;}
    const char *cmd="printf 'WB_PTY_OK\\n'\n";
    if(wb_terminal_send(&s,cmd,strlen(cmd))<0)die("pty send");
    struct timespec ts={0,20000000};
    int ok=0;
    for(int i=0;i<150;i++){
        wb_terminal_pump(&s);
        if(contains(&s,"WB_PTY_OK")){ok=1;break;}
        nanosleep(&ts,NULL);
    }
    if(!ok)die("pty output not captured");
    wb_terminal_end(&s);
    wb_terminal_destroy(&s);
    puts("TERMINAL CORE PASS");
    return 0;
}
