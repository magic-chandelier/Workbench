#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <ctype.h>
#include <fcntl.h>
#include <grp.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))
#define WB_VERSION "0.15.0"
#define KEY_UP 1000
#define KEY_DOWN 1001
#define KEY_LEFT 1002
#define KEY_RIGHT 1003
#define KEY_MOUSE 1004
#define MAX_QUERY 160
#define MAX_COMMAND 8192
#define WB_INPUT 768

static struct termios original_termios;
static int raw_enabled = 0;
static int mouse_x = 0, mouse_y = 0, mouse_button = -1, mouse_release = 0;
static size_t main_visible_start = 0, main_visible_end = 0;
static const int main_row_start_y = 8;
static const int main_exec_x1 = 41, main_exec_x2 = 48;
static const int main_prop_x1 = 50, main_prop_x2 = 57;

#include "workbench.h"
#include "modules/linux-core/linux_core.h"
#include "modules/centos/centos.h"
#include "modules/files-manager/files_manager.h"
#include "modules/terminal/terminal.h"
#include "modules/command-sets/command_sets.h"

typedef struct {
    Language language;
    int confirm_normal;
    int confirm_sensitive;
    int confirm_privileged;
    LinuxProfile linux_profile;
    int virtual_keyboard;
    int shortcut_hints;
    int terminal_log_lines;
    int terminal_load_lines;
    int terminal_clear_on_open;
} AppConfig;

static int g_virtual_keyboard_enabled = 0;
static int g_shortcut_hints_enabled = 1;
static WbTerminalSession g_terminal_session;
static int g_terminal_session_initialized = 0;
static CommandSetStore g_command_sets;
static int g_command_sets_initialized = 0;

typedef enum { COMMAND_MODE_EXECUTE = 0, COMMAND_MODE_INSERT = 1 } CommandBrowserMode;
static int command_sets_menu(AppConfig *c, CommandBrowserMode mode, char *insert_out, size_t insert_cap);

typedef struct {
    LinuxProfile profile;
    char id[64];
    char version[64];
    char pretty_name[160];
} DetectedLinux;

#define WB_MAX_EFFECTIVE_TASKS 1024
static Task g_effective_tasks[WB_MAX_EFFECTIVE_TASKS];
static size_t g_effective_task_count = 0;
static LinuxProfile g_runtime_profile = PROFILE_GENERIC;
static size_t g_overlay_override_count = 0;

static size_t module_count(void) { return 1; }
static const Module *module_at(size_t index) { return index == 0 ? linux_core_module() : NULL; }
static size_t total_module_actions(void) { return g_effective_task_count; }
#define TASKS (g_effective_tasks)
#define TASK_COUNT (g_effective_task_count)

static int task_index_by_id_raw(const Task *tasks,size_t count,const char *id){
    for(size_t i=0;i<count;i++)if(tasks[i].id&&strcmp(tasks[i].id,id)==0)return (int)i;
    return -1;
}
static int rebuild_effective_catalogue(LinuxProfile profile){
    const Module *core=linux_core_module();
    if(core->task_count>WB_MAX_EFFECTIVE_TASKS)return -1;
    memcpy(g_effective_tasks,core->tasks,core->task_count*sizeof(Task));
    g_effective_task_count=core->task_count;g_overlay_override_count=0;g_runtime_profile=profile;
    if(profile==PROFILE_CENTOS){
        const Module *overlay=centos_module();
        for(size_t i=0;i<overlay->task_count;i++){
            const Task *t=&overlay->tasks[i];int idx=task_index_by_id_raw(g_effective_tasks,g_effective_task_count,t->id);
            if(idx>=0){g_effective_tasks[idx]=*t;g_overlay_override_count++;}
            else{if(g_effective_task_count>=WB_MAX_EFFECTIVE_TASKS)return -1;g_effective_tasks[g_effective_task_count++]=*t;}
        }
    }
    return 0;
}


static const char *tr(Language l, const char *zh, const char *en) { return l == LANG_ZH ? zh : en; }
static void ui_hint_line(Language l,const char *zh,const char *en){if(g_shortcut_hints_enabled)printf("%s\n",tr(l,zh,en));else putchar('\n');}
static const char *task_title(const Task *t, Language l) { return l == LANG_ZH ? t->title_zh : t->title_en; }
static const char *task_desc(const Task *t, Language l) { return l == LANG_ZH ? t->desc_zh : t->desc_en; }

static const char *category_name(Category c, Language l) {
    static const char *zh[] = {"全部","系统","文件","文本","进程","网络","存储","权限","归档","用户","软件包","服务","防火墙","SELinux"};
    static const char *en[] = {"All","System","Files","Text","Process","Network","Storage","Perms","Archive","Users","Packages","Services","Firewall","SELinux"};
    return l == LANG_ZH ? zh[c] : en[c];
}
static const char *task_source_name(const Task*t,Language l){
    if(t->source_id&&!strcmp(t->source_id,"centos"))return "CentOS";
    if(t->source_id&&!strcmp(t->source_id,"user"))return tr(l,"用户自定义","User Custom");
    return tr(l,"Linux 通用","Linux Core");
}
static const char *task_source_marker(const Task*t){
    if(t->source_id&&!strcmp(t->source_id,"centos"))return "CentOS";
    if(t->source_id&&!strcmp(t->source_id,"user"))return "Custom";
    return "";
}
static const char *effective_workbench_title(Language l){return g_runtime_profile==PROFILE_CENTOS?tr(l,"CentOS Linux 指令集","CentOS Linux Command Set"):tr(l,"Linux 通用指令集","Linux Generic Command Set");}
static const char *effective_workbench_desc(Language l){return g_runtime_profile==PROFILE_CENTOS?tr(l,"Linux 通用指令 + CentOS Stream 9/10 增强","Linux generic actions + CentOS Stream 9/10 overlay"):tr(l,"跨发行版 Linux 通用操作","Cross-distro generic Linux operations");}
static const char *risk_name(Risk r, Language l) {
    if (r == RISK_NORMAL) return tr(l,"普通","Normal");
    if (r == RISK_SENSITIVE) return tr(l,"敏感","Sensitive");
    return tr(l,"特权","Privileged");
}

static void restore_terminal(void) {
    if (raw_enabled) { tcsetattr(STDIN_FILENO,TCSAFLUSH,&original_termios); raw_enabled=0; }
    printf("\033[?1000l\033[?1006l\033[?25h\033[0m"); fflush(stdout);
}
static void fatal_signal(int sig) { if(g_terminal_session_initialized&&wb_terminal_is_running(&g_terminal_session)&&g_terminal_session.child_pid>0)kill(-g_terminal_session.child_pid,SIGHUP);restore_terminal();signal(sig,SIG_DFL);raise(sig); }
static int enable_raw_mode(void) {
    if (!isatty(STDIN_FILENO)) { fprintf(stderr,"Workbench requires an interactive terminal.\n"); return -1; }
    if (tcgetattr(STDIN_FILENO,&original_termios)==-1) return -1;
    struct termios raw=original_termios;
    raw.c_lflag &= (tcflag_t)~(ECHO|ICANON|IEXTEN);
    raw.c_iflag &= (tcflag_t)~(IXON|ICRNL);
    raw.c_cc[VMIN]=1; raw.c_cc[VTIME]=0;
    if (tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw)==-1) return -1;
    raw_enabled=1; printf("\033[?1000h\033[?1006h"); fflush(stdout); return 0;
}
static void terminal_passthrough_signals(int enabled){
    if(!raw_enabled)return;
    struct termios t;
    if(tcgetattr(STDIN_FILENO,&t)==-1)return;
    if(enabled)t.c_lflag&=(tcflag_t)~ISIG;
    else if(original_termios.c_lflag&ISIG)t.c_lflag|=ISIG;
    else t.c_lflag&=(tcflag_t)~ISIG;
    tcsetattr(STDIN_FILENO,TCSANOW,&t);
}
static void clear_screen(void) { printf("\033[2J\033[H"); }
static int terminal_width(void) { struct winsize w; return ioctl(STDOUT_FILENO,TIOCGWINSZ,&w)==0&&w.ws_col? w.ws_col:80; }
static int terminal_height(void) { struct winsize w; return ioctl(STDOUT_FILENO,TIOCGWINSZ,&w)==0&&w.ws_row? w.ws_row:24; }
static void print_rule(int n) { if(n<20)n=20; while(n--) putchar('-'); putchar('\n'); }

static size_t utf8_decode_one(const unsigned char *s, unsigned int *cp) {
    unsigned char c=s[0]; if(c<0x80){*cp=c;return 1;}
    if((c&0xe0)==0xc0&&(s[1]&0xc0)==0x80){unsigned v=((c&0x1f)<<6)|(s[1]&0x3f);if(v>=0x80){*cp=v;return 2;}}
    if((c&0xf0)==0xe0&&(s[1]&0xc0)==0x80&&(s[2]&0xc0)==0x80){unsigned v=((c&0x0f)<<12)|((s[1]&0x3f)<<6)|(s[2]&0x3f);if(v>=0x800&&!(v>=0xd800&&v<=0xdfff)){*cp=v;return 3;}}
    if((c&0xf8)==0xf0&&(s[1]&0xc0)==0x80&&(s[2]&0xc0)==0x80&&(s[3]&0xc0)==0x80){unsigned v=((c&7)<<18)|((s[1]&0x3f)<<12)|((s[2]&0x3f)<<6)|(s[3]&0x3f);if(v>=0x10000&&v<=0x10ffff){*cp=v;return 4;}}
    *cp=c; return 1;
}
static int unicode_cell_width(unsigned cp) {
    if(cp==0||cp<0x20||(cp>=0x7f&&cp<0xa0)) return 0;
    if((cp>=0x300&&cp<=0x36f)||(cp>=0x1ab0&&cp<=0x1aff)||(cp>=0x1dc0&&cp<=0x1dff)||(cp>=0x20d0&&cp<=0x20ff)||(cp>=0xfe00&&cp<=0xfe0f)||cp==0x200b||cp==0x200c||cp==0x200d) return 0;
    if((cp>=0x1100&&cp<=0x115f)||(cp>=0x2e80&&cp<=0xa4cf)||(cp>=0xac00&&cp<=0xd7a3)||(cp>=0xf900&&cp<=0xfaff)||(cp>=0xfe10&&cp<=0xfe6f)||(cp>=0xff00&&cp<=0xff60)||(cp>=0xffe0&&cp<=0xffe6)||(cp>=0x1f300&&cp<=0x1faff)||(cp>=0x20000&&cp<=0x3fffd)) return 2;
    return 1;
}
static int display_width(const char *s){int w=0;const unsigned char*p=(const unsigned char*)s;while(*p){unsigned cp;size_t n=utf8_decode_one(p,&cp);w+=unicode_cell_width(cp);p+=n;}return w;}
static int print_truncated(const char*s,int cols){int used=0;const unsigned char*p=(const unsigned char*)s;while(*p&&used<cols){unsigned cp;size_t n=utf8_decode_one(p,&cp);int w=unicode_cell_width(cp);if(used+w>cols)break;fwrite(p,1,n,stdout);used+=w;p+=n;}return used;}
static void print_cell(const char*s,int cols){int u=print_truncated(s,cols);while(u++<cols)putchar(' ');}
static void print_padded(const char*s,int cols){fputs(s,stdout);int p=cols-display_width(s);while(p-->0)putchar(' ');}

typedef enum {
    UI_EVENT_EOF = 0,
    UI_EVENT_KEY,
    UI_EVENT_MOUSE_PRESS,
    UI_EVENT_MOUSE_RELEASE,
    UI_EVENT_WHEEL_UP,
    UI_EVENT_WHEEL_DOWN
} UiEventKind;

typedef struct {
    UiEventKind kind;
    int key;
    int x;
    int y;
    int button;
} UiEvent;

typedef struct {
    int id;
    int x1, y1, x2, y2;
} UiHitRegion;

#define UI_MAX_HITS 128
static UiHitRegion ui_hits[UI_MAX_HITS];
static size_t ui_hit_count = 0;
static int ui_pending_valid = 0;
static UiEvent ui_pending_event;

static void ui_hits_reset(void) { ui_hit_count = 0; }
static void ui_hit_add(int id,int x1,int y1,int x2,int y2){
    if(ui_hit_count>=UI_MAX_HITS||x2<x1||y2<y1)return;
    ui_hits[ui_hit_count++]=(UiHitRegion){id,x1,y1,x2,y2};
}
static int ui_hit_test(const UiEvent *ev){
    if(!ev||(ev->kind!=UI_EVENT_MOUSE_PRESS&&ev->kind!=UI_EVENT_MOUSE_RELEASE))return -1;
    for(size_t i=ui_hit_count;i>0;i--){const UiHitRegion*r=&ui_hits[i-1];if(ev->x>=r->x1&&ev->x<=r->x2&&ev->y>=r->y1&&ev->y<=r->y2)return r->id;}
    return -1;
}

static UiEvent ui_read_event_ready(void) {
    if(ui_pending_valid){ui_pending_valid=0;return ui_pending_event;}
    UiEvent ev={UI_EVENT_EOF,-1,0,0,-1};
    unsigned char c;if(read(STDIN_FILENO,&c,1)<=0)return ev;
    if(c!='\033'){ev.kind=UI_EVENT_KEY;ev.key=c;return ev;}
    struct pollfd p={STDIN_FILENO,POLLIN,0};unsigned char a,b;
    if(poll(&p,1,35)<=0||read(STDIN_FILENO,&a,1)!=1){ev.kind=UI_EVENT_KEY;ev.key=27;return ev;}
    if(a!='['){ev.kind=UI_EVENT_KEY;ev.key=27;return ev;}
    if(poll(&p,1,35)<=0||read(STDIN_FILENO,&b,1)!=1){ev.kind=UI_EVENT_KEY;ev.key=27;return ev;}
    if(b=='A'){ev.kind=UI_EVENT_KEY;ev.key=KEY_UP;return ev;}
    if(b=='B'){ev.kind=UI_EVENT_KEY;ev.key=KEY_DOWN;return ev;}
    if(b=='C'){ev.kind=UI_EVENT_KEY;ev.key=KEY_RIGHT;return ev;}
    if(b=='D'){ev.kind=UI_EVENT_KEY;ev.key=KEY_LEFT;return ev;}
    if(b=='<'){
        char buf[48];size_t n=0;unsigned char ch=0;
        while(n+1<sizeof(buf)){if(poll(&p,1,35)<=0||read(STDIN_FILENO,&ch,1)!=1){ev.kind=UI_EVENT_KEY;ev.key=27;return ev;}if(ch=='M'||ch=='m')break;buf[n++]=(char)ch;}buf[n]='\0';
        int mb=-1,x=0,y=0;
        if(sscanf(buf,"%d;%d;%d",&mb,&x,&y)==3){ev.x=x;ev.y=y;ev.button=mb&3;if(mb&64){ev.kind=(mb&1)?UI_EVENT_WHEEL_DOWN:UI_EVENT_WHEEL_UP;return ev;}ev.kind=(ch=='m')?UI_EVENT_MOUSE_RELEASE:UI_EVENT_MOUSE_PRESS;return ev;}
    }
    ev.kind=UI_EVENT_KEY;ev.key=27;return ev;
}

static UiEvent ui_read_event(void) {
    if(ui_pending_valid)return ui_read_event_ready();
    for(;;){
        struct pollfd fds[2];nfds_t nfds=1;fds[0]=(struct pollfd){STDIN_FILENO,POLLIN,0};
        int tfd=(g_terminal_session_initialized&&wb_terminal_is_running(&g_terminal_session))?wb_terminal_master_fd(&g_terminal_session):-1;
        if(tfd>=0){fds[nfds++]=(struct pollfd){tfd,POLLIN|POLLHUP|POLLERR,0};}
        int rc=poll(fds,nfds,-1);if(rc<0){if(errno==EINTR)continue;UiEvent ev={UI_EVENT_EOF,-1,0,0,-1};return ev;}
        if(nfds>1&&fds[1].revents)wb_terminal_pump(&g_terminal_session);
        if(fds[0].revents&POLLIN)return ui_read_event_ready();
    }
}

static int read_key(void) {
    UiEvent ev=ui_read_event();
    if(ev.kind==UI_EVENT_EOF)return -1;
    if(ev.kind==UI_EVENT_KEY)return ev.key;
    if(ev.kind==UI_EVENT_WHEEL_UP)return KEY_UP;
    if(ev.kind==UI_EVENT_WHEEL_DOWN)return KEY_DOWN;
    mouse_button=ev.button;mouse_x=ev.x;mouse_y=ev.y;mouse_release=ev.kind==UI_EVENT_MOUSE_RELEASE;
    return KEY_MOUSE;
}
static void ui_consume_optional_newline(void){
    struct pollfd p={STDIN_FILENO,POLLIN,0};
    if(poll(&p,1,8)>0){UiEvent ev=ui_read_event();if(!(ev.kind==UI_EVENT_KEY&&(ev.key=='\r'||ev.key=='\n'))){ui_pending_event=ev;ui_pending_valid=1;}}
}

static int ensure_dir(const char *p){return mkdir(p,0700)==0||errno==EEXIST?0:-1;}
static int config_path(char*b,size_t n){const char*h=getenv("HOME");return h&&*h&&snprintf(b,n,"%s/.config/workbench/config",h)<(int)n?0:-1;}
static int ensure_config_dir(void){const char*h=getenv("HOME");char p[PATH_MAX];if(!h||!*h)return-1;if(snprintf(p,sizeof(p),"%s/.config",h)>=(int)sizeof(p)||ensure_dir(p))return-1;if(snprintf(p,sizeof(p),"%s/.config/workbench",h)>=(int)sizeof(p))return-1;return ensure_dir(p);}
static void config_defaults(AppConfig*c){c->language=LANG_ZH;c->confirm_normal=0;c->confirm_sensitive=0;c->confirm_privileged=1;c->linux_profile=PROFILE_GENERIC;c->virtual_keyboard=0;c->shortcut_hints=1;c->terminal_log_lines=1000;c->terminal_load_lines=50;c->terminal_clear_on_open=0;}
static void apply_ui_preferences(const AppConfig*c){g_virtual_keyboard_enabled=c->virtual_keyboard;g_shortcut_hints_enabled=c->shortcut_hints;}
static void load_config(AppConfig*c){char p[PATH_MAX],l[128];config_defaults(c);if(config_path(p,sizeof(p))){apply_ui_preferences(c);return;}FILE*f=fopen(p,"r");if(!f){apply_ui_preferences(c);return;}while(fgets(l,sizeof(l),f)){if(!strncmp(l,"language=en",11))c->language=LANG_EN;else if(!strncmp(l,"language=zh",11))c->language=LANG_ZH;else if(!strncmp(l,"confirm_normal=1",16))c->confirm_normal=1;else if(!strncmp(l,"confirm_normal=0",16))c->confirm_normal=0;else if(!strncmp(l,"confirm_sensitive=1",19))c->confirm_sensitive=1;else if(!strncmp(l,"confirm_sensitive=0",19))c->confirm_sensitive=0;else if(!strncmp(l,"confirm_privileged=1",20))c->confirm_privileged=1;else if(!strncmp(l,"confirm_privileged=0",20))c->confirm_privileged=0;else if(!strncmp(l,"linux_profile=centos",20))c->linux_profile=PROFILE_CENTOS;else if(!strncmp(l,"linux_profile=generic",21))c->linux_profile=PROFILE_GENERIC;else if(!strncmp(l,"virtual_keyboard=1",18))c->virtual_keyboard=1;else if(!strncmp(l,"virtual_keyboard=0",18))c->virtual_keyboard=0;else if(!strncmp(l,"shortcut_hints=1",16))c->shortcut_hints=1;else if(!strncmp(l,"shortcut_hints=0",16))c->shortcut_hints=0;else if(!strncmp(l,"terminal_log_lines=",19)){int v=atoi(l+19);if(v>=100&&v<=10000)c->terminal_log_lines=v;}else if(!strncmp(l,"terminal_load_lines=",20)){int v=atoi(l+20);if(v>=10&&v<=500)c->terminal_load_lines=v;}else if(!strncmp(l,"terminal_clear_on_open=1",24))c->terminal_clear_on_open=1;else if(!strncmp(l,"terminal_clear_on_open=0",24))c->terminal_clear_on_open=0;}fclose(f);apply_ui_preferences(c);}
static const char *linux_profile_key(LinuxProfile p){return p==PROFILE_CENTOS?"centos":"generic";}
static int save_config(const AppConfig*c){char p[PATH_MAX];apply_ui_preferences(c);if(ensure_config_dir()||config_path(p,sizeof(p)))return-1;FILE*f=fopen(p,"w");if(!f)return-1;fprintf(f,"language=%s\nconfirm_normal=%d\nconfirm_sensitive=%d\nconfirm_privileged=%d\nlinux_profile=%s\nvirtual_keyboard=%d\nshortcut_hints=%d\nterminal_log_lines=%d\nterminal_load_lines=%d\nterminal_clear_on_open=%d\n",c->language==LANG_ZH?"zh":"en",c->confirm_normal,c->confirm_sensitive,c->confirm_privileged,linux_profile_key(c->linux_profile),c->virtual_keyboard,c->shortcut_hints,c->terminal_log_lines,c->terminal_load_lines,c->terminal_clear_on_open);return fclose(f);}
static int confirmation_enabled(const AppConfig*c,Risk r){return r==RISK_PRIVILEGED?c->confirm_privileged:r==RISK_SENSITIVE?c->confirm_sensitive:c->confirm_normal;}
static void copy_os_value(char *dst,size_t n,const char *src){
    size_t z=strcspn(src,"\r\n");
    while(z&&isspace((unsigned char)src[z-1]))z--;
    size_t st=0;while(st<z&&isspace((unsigned char)src[st]))st++;
    if(z>st+1&&((src[st]=='"'&&src[z-1]=='"')||(src[st]=='\''&&src[z-1]=='\''))){st++;z--;}
    size_t len=z>st?z-st:0;if(len>=n)len=n-1;memcpy(dst,src+st,len);dst[len]='\0';
}
static void detect_linux(DetectedLinux*d){
    memset(d,0,sizeof(*d));d->profile=PROFILE_GENERIC;strcpy(d->id,"unknown");
    const char *path=getenv("WB_OS_RELEASE");if(!path||!*path)path="/etc/os-release";
    FILE*f=fopen(path,"r");if(!f)return;char line[512];
    while(fgets(line,sizeof(line),f)){
        char *eq=strchr(line,'=');if(!eq)continue;*eq='\0';const char *v=eq+1;
        if(!strcmp(line,"ID"))copy_os_value(d->id,sizeof(d->id),v);
        else if(!strcmp(line,"VERSION_ID"))copy_os_value(d->version,sizeof(d->version),v);
        else if(!strcmp(line,"PRETTY_NAME"))copy_os_value(d->pretty_name,sizeof(d->pretty_name),v);
    }
    fclose(f);if(!strcmp(d->id,"centos"))d->profile=PROFILE_CENTOS;
}
static LinuxProfile effective_linux_profile(const AppConfig*c,const DetectedLinux*d){(void)d;return c->linux_profile;}
static const char *linux_profile_label(LinuxProfile p,Language l){
    if(p==PROFILE_CENTOS)return "CentOS";
    return tr(l,"Linux 通用","Linux Generic");
}
static int rebuild_from_config(const AppConfig*c){
    DetectedLinux d;detect_linux(&d);int rc=rebuild_effective_catalogue(effective_linux_profile(c,&d));
    if(!rc&&g_command_sets_initialized){g_command_sets.sets[0].system_tasks=TASKS;g_command_sets.sets[0].system_task_count=TASK_COUNT;}
    return rc;
}

static const char *detect_shell_rc(void){static char p[PATH_MAX];const char*h=getenv("HOME"),*s=getenv("SHELL");if(!h||!*h)return NULL;if(s&&strstr(s,"zsh"))snprintf(p,sizeof(p),"%s/.zshrc",h);else if(s&&strstr(s,"bash"))snprintf(p,sizeof(p),"%s/.bashrc",h);else snprintf(p,sizeof(p),"%s/.profile",h);return p;}
static int self_path(char*b,size_t n){ssize_t k=readlink("/proc/self/exe",b,n-1);if(k<=0)return-1;b[k]='\0';return 0;}
static int file_contains(const char*p,const char*n){FILE*f=fopen(p,"r");if(!f)return 0;char l[2048];int ok=0;while(fgets(l,sizeof(l),f))if(strstr(l,n)){ok=1;break;}fclose(f);return ok;}
static int autostart_enabled(void){const char*p=detect_shell_rc();return p?file_contains(p,"# Workbench autostart begin"):0;}
static int shell_quote(const char*s,char*d,size_t n){size_t u=0;if(n<3)return-1;d[u++]='\'';for(;*s;s++){if(*s=='\''){if(u+4>=n)return-1;memcpy(d+u,"'\\''",4);u+=4;}else{if(u+1>=n)return-1;d[u++]=*s;}}if(u+2>n)return-1;d[u++]='\'';d[u]='\0';return 0;}
static int enable_autostart(void){const char*rc=detect_shell_rc();char e[PATH_MAX],q[PATH_MAX*2];if(!rc||self_path(e,sizeof(e))||shell_quote(e,q,sizeof(q)))return-1;if(autostart_enabled())return 0;FILE*f=fopen(rc,"a");if(!f)return-1;fprintf(f,"\n# Workbench autostart begin\nif [ -t 0 ] && [ -t 1 ] && [ -z \"${WB_AUTOSTART_ACTIVE:-}\" ] && [ -z \"${WB_AUTOSTART_DISABLE:-}\" ]; then\n export WB_AUTOSTART_ACTIVE=1\n %s\n unset WB_AUTOSTART_ACTIVE\nfi\n# Workbench autostart end\n",q);return fclose(f);}
static int disable_autostart(void){const char*rc=detect_shell_rc();if(!rc)return-1;FILE*in=fopen(rc,"r");if(!in)return 0;char tmp[PATH_MAX],l[2048];snprintf(tmp,sizeof(tmp),"%s.workbench.tmp",rc);FILE*out=fopen(tmp,"w");if(!out){fclose(in);return-1;}int skip=0;while(fgets(l,sizeof(l),in)){if(strstr(l,"# Workbench autostart begin")){skip=1;continue;}if(skip&&strstr(l,"# Workbench autostart end")){skip=0;continue;}if(!skip)fputs(l,out);}fclose(in);if(fclose(out)){unlink(tmp);return-1;}return rename(tmp,rc);}

static void ui_print_button(int id,int y,int *x,const char *label){
    int start=*x;printf("[%s] ",label);int width=display_width(label)+2;ui_hit_add(id,start,y,start+width-1,y);*x=start+width+1;
}
static void ui_utf8_backspace(char*s){size_t n=strlen(s);if(!n)return;n--;while(n&&(((unsigned char)s[n]&0xc0)==0x80))n--;s[n]='\0';}
static int ui_append_byte(char *buf,size_t cap,unsigned char c){size_t n=strlen(buf);if(n+1>=cap)return 0;buf[n]=(char)c;buf[n+1]='\0';return 1;}

static void ui_wait_return_inline(Language l){
    int y=terminal_height();if(y<1)y=1;printf("\033[%d;1H",y);ui_hits_reset();int x=1;ui_print_button(1,y,&x,tr(l,"返回","Return"));if(g_shortcut_hints_enabled)printf(" %s",tr(l,"Enter/q 也可返回","Enter/q also returns"));fflush(stdout);
    for(;;){UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)return;if(ev.kind==UI_EVENT_KEY&&(ev.key=='\r'||ev.key=='\n'||ev.key=='q'||ev.key=='Q'||ev.key==27))return;if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0&&ui_hit_test(&ev)==1)return;}
}

static void show_message(Language l,const char*zh,const char*en){
    for(;;){int w=terminal_width();if(w>100)w=100;clear_screen();ui_hits_reset();printf("\033[?25lWorkbench :: %s\n",tr(l,"提示","Message"));print_rule(w);printf("%s\n\n",tr(l,zh,en));int y=5,x=2;ui_print_button(1,y,&x,tr(l,"返回","Back"));putchar('\n');if(g_shortcut_hints_enabled)printf("%s\n",tr(l,"键盘：Enter/Esc/q 返回；鼠标：点击返回","Keyboard: Enter/Esc/q back; Mouse: click Back"));fflush(stdout);UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)return;if(ev.kind==UI_EVENT_KEY&&(ev.key=='\r'||ev.key=='\n'||ev.key==27||ev.key=='q'||ev.key=='Q'))return;if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0&&ui_hit_test(&ev)==1)return;}
}

static int ui_confirm_dialog(Language l,const char *title,const char *body,const char *yes_label,const char *no_label){
    for(;;){int w=terminal_width();if(w>110)w=110;clear_screen();ui_hits_reset();printf("\033[?25lWorkbench :: %s\n",title);print_rule(w);printf("%s\n\n",body);int y=5,x=2;ui_print_button(1,y,&x,yes_label);ui_print_button(2,y,&x,no_label);putchar('\n');if(g_shortcut_hints_enabled)printf("%s\n",tr(l,"键盘：y 确认，n/Enter/Esc 取消；鼠标：点击按钮","Keyboard: y confirms, n/Enter/Esc cancels; Mouse: click a button"));fflush(stdout);UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)return 0;if(ev.kind==UI_EVENT_KEY){if(ev.key=='y'||ev.key=='Y'){ui_consume_optional_newline();return 1;}if(ev.key=='n'||ev.key=='N'){ui_consume_optional_newline();return 0;}if(ev.key=='\r'||ev.key=='\n'||ev.key==27||ev.key=='q'||ev.key=='Q')return 0;}if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0){int hit=ui_hit_test(&ev);if(hit==1)return 1;if(hit==2)return 0;}}
}
static int confirm_line(Language l,const char*zh,const char*en){return ui_confirm_dialog(l,tr(l,"确认操作","Confirm action"),tr(l,zh,en),tr(l,"确认","Confirm"),tr(l,"取消","Cancel"));}
static int confirm_command(Language l,const Task*t,const char*cmd){
    char body[MAX_COMMAND+768];snprintf(body,sizeof(body),"%s: %s\n%s: %s\n%s: %s",tr(l,"功能","Task"),task_title(t,l),tr(l,"风险","Risk"),risk_name(t->risk,l),tr(l,"将执行","Command"),cmd);
    return ui_confirm_dialog(l,tr(l,"执行确认","Run confirmation"),body,tr(l,"执行","Run"),tr(l,"取消","Cancel"));
}
static void run_command(const Task*t,Language l,const char*cmd){
    restore_terminal();clear_screen();printf("Workbench :: %s\n%s\n%s: %s\n\n",task_title(t,l),task_desc(t,l),tr(l,"执行命令","Command"),cmd);fflush(stdout);pid_t p=fork();int st=0;
    if(p==0){execl("/bin/sh","sh","-c",cmd,(char*)NULL);_exit(127);}if(p<0){perror("fork");}else{while(waitpid(p,&st,0)==-1&&errno==EINTR){}if(WIFEXITED(st)&&WEXITSTATUS(st))printf("\n[%s %d]\n",tr(l,"命令退出码","command exited with code"),WEXITSTATUS(st));else if(WIFSIGNALED(st))printf("\n[%s %d]\n",tr(l,"命令被信号终止","command terminated by signal"),WTERMSIG(st));}
    fflush(stdout);if(enable_raw_mode())exit(1);ui_wait_return_inline(l);
}

#define UI_VK_CHAR_BASE 1000
#define UI_VK_SHIFT 2001
#define UI_VK_SPACE 2002
#define UI_VK_BACKSPACE 2003
#define UI_VK_CLEAR 2004
#define UI_VK_ACCEPT 2005
#define UI_VK_CANCEL 2006

static void ui_draw_vk_row(const char *keys,int y,int shift){
    int x=2;for(const unsigned char*p=(const unsigned char*)keys;*p;p++){unsigned char c=*p;if(shift&&c>='a'&&c<='z')c=(unsigned char)toupper(c);char label[2]={(char)c,'\0'};ui_print_button(UI_VK_CHAR_BASE+(int)c,y,&x,label);}
}
static int ui_input_dialog(Language l,const char*title,const char*prompt,const char*def,char*out,size_t n,int allow_empty){
    char buf[WB_INPUT]="";int shift=0;
    for(;;){int w=terminal_width();if(w>110)w=110;clear_screen();ui_hits_reset();printf("\033[?25lWorkbench :: %s\n",title);print_rule(w);printf("%s\n",prompt);if(def&&*def)printf("%s: %s\n",tr(l,"默认值","Default"),def);else putchar('\n');
        printf("%s: ",tr(l,"输入","Input"));print_truncated(buf,w-10);printf("_\n\n");
        if(g_virtual_keyboard_enabled){
            ui_draw_vk_row("1234567890-_=" ,7,shift);ui_draw_vk_row("qwertyuiop[]\\",8,shift);ui_draw_vk_row("asdfghjkl;'\"",9,shift);ui_draw_vk_row("zxcvbnm,./:",10,shift);ui_draw_vk_row("~@+$%&!*?|<>",11,shift);
            int x=2;ui_print_button(UI_VK_SHIFT,12,&x,"Shift");ui_print_button(UI_VK_SPACE,12,&x,tr(l,"空格","Space"));ui_print_button(UI_VK_BACKSPACE,12,&x,tr(l,"退格","Backspace"));ui_print_button(UI_VK_CLEAR,12,&x,tr(l,"清空","Clear"));
            x=2;ui_print_button(UI_VK_ACCEPT,13,&x,tr(l,"确定","OK"));ui_print_button(UI_VK_CANCEL,13,&x,tr(l,"取消","Cancel"));
            printf("\n%s: %s",tr(l,"Shift 状态","Shift state"),shift?tr(l,"开启","ON"):tr(l,"关闭","OFF"));if(g_shortcut_hints_enabled)printf("   %s",tr(l,"键盘可直接输入；Enter 确定，Esc 取消。鼠标可使用上方虚拟键盘。","Type directly with keyboard; Enter accepts, Esc cancels. Mouse can use the on-screen keyboard."));putchar('\n');
        }else{
            printf("%s\n\n",tr(l,"虚拟键盘：已关闭（可在设置中开启，键盘仍可直接输入）","Virtual keyboard: Disabled (enable it in Settings for mouse text entry; physical keyboard input still works)"));
            int x=2;ui_print_button(UI_VK_ACCEPT,9,&x,tr(l,"确定","OK"));ui_print_button(UI_VK_CANCEL,9,&x,tr(l,"取消","Cancel"));
            if(g_shortcut_hints_enabled)printf("\n%s\n",tr(l,"快捷键：Enter 确定，Esc 取消，Backspace 删除，Ctrl+U 清空。","Shortcuts: Enter accept, Esc cancel, Backspace delete, Ctrl+U clear."));
        }
        fflush(stdout);UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)return 0;
        if(ev.kind==UI_EVENT_KEY){int k=ev.key;if(k==27)return 0;if(k=='\r'||k=='\n'){const char*value=*buf?buf:(def?def:"");if((allow_empty||*value)&&strlen(value)+1<=n){strcpy(out,value);return 1;}continue;}if(k==127||k==8){ui_utf8_backspace(buf);continue;}if(k==21){buf[0]='\0';continue;}if(k>=32&&k<=255){ui_append_byte(buf,sizeof(buf),(unsigned char)k);continue;}}
        if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0){int hit=ui_hit_test(&ev);if(g_virtual_keyboard_enabled&&hit>=UI_VK_CHAR_BASE&&hit<UI_VK_CHAR_BASE+256){ui_append_byte(buf,sizeof(buf),(unsigned char)(hit-UI_VK_CHAR_BASE));continue;}if(g_virtual_keyboard_enabled&&hit==UI_VK_SHIFT){shift=!shift;continue;}if(g_virtual_keyboard_enabled&&hit==UI_VK_SPACE){ui_append_byte(buf,sizeof(buf),' ');continue;}if(g_virtual_keyboard_enabled&&hit==UI_VK_BACKSPACE){ui_utf8_backspace(buf);continue;}if(g_virtual_keyboard_enabled&&hit==UI_VK_CLEAR){buf[0]='\0';continue;}if(hit==UI_VK_CANCEL)return 0;if(hit==UI_VK_ACCEPT){const char*value=*buf?buf:(def?def:"");if((allow_empty||*value)&&strlen(value)+1<=n){strcpy(out,value);return 1;}}}
    }
}

static int destructive_path_allowed(const char *s) {
    if (!s || !*s) return 0;
    size_t n = strlen(s);
    while (n > 1 && s[n - 1] == '/') n--;
    if ((n == 1 && s[0] == '/') || (n == 1 && s[0] == '.') ||
        (n == 2 && s[0] == '.' && s[1] == '.')) return 0;
    int all_slash = 1;
    for (size_t i = 0; i < n; ++i) if (s[i] != '/') { all_slash = 0; break; }
    if (all_slash) return 0;
    char resolved[PATH_MAX];
    if (realpath(s, resolved) && strcmp(resolved, "/") == 0) return 0;
    return 1;
}

static int validate_arg(ArgKind k,const char*s,Language l){
    if(k==ARG_NONE||k==ARG_TEXT||k==ARG_PATH)return 1;
    if(k==ARG_DESTRUCTIVE_PATH){
        if(destructive_path_allowed(s))return 1;
        show_message(l,"为避免误伤，破坏性递归操作不能直接以 /、//、. 或 .. 作为目标。请填写更具体的路径。",
                       "For safety, destructive recursive actions cannot target /, //, . or ... Enter a more specific path.");
        return 0;
    }
    if(!s||!*s){show_message(l,"输入不能为空。","Input cannot be empty.");return 0;}
    for(const char*p=s;*p;p++)if(!isdigit((unsigned char)*p)){show_message(l,"此参数必须是正整数。","This parameter must be a positive integer.");return 0;}
    long v=strtol(s,NULL,10);if(k==ARG_PORT&&(v<1||v>65535)){show_message(l,"端口范围必须是 1-65535。","Port must be 1-65535.");return 0;}if((k==ARG_PID||k==ARG_UINT)&&v<1){show_message(l,"数值必须大于 0。","Value must be greater than 0.");return 0;}return 1;
}
static int prompt_text(Language l,const char*title,const char*pzh,const char*pen,const char*def,ArgKind k,char*out,size_t n){
    if(!ui_input_dialog(l,title,tr(l,pzh,pen),def,out,n,0)) return 0;
    return validate_arg(k,out,l);
}
static int substitute_template(const char *tmpl, char quoted[WB_MAX_ARGS][WB_INPUT * 4], char *out, size_t n) {
    size_t u = 0;
    for (size_t i = 0; tmpl[i];) {
        const char *replacement = NULL;
        size_t skip = 1;
        if (tmpl[i] == '{' && tmpl[i + 1] >= '1' && tmpl[i + 1] <= '4' && tmpl[i + 2] == '}') {
            size_t index = (size_t)(tmpl[i + 1] - '1');
            replacement = quoted[index][0] ? quoted[index] : "''";
            skip = 3;
        }
        if (replacement) {
            size_t m = strlen(replacement);
            if (u + m + 1 > n) return 0;
            memcpy(out + u, replacement, m);
            u += m;
            i += skip;
        } else {
            if (u + 2 > n) return 0;
            out[u++] = tmpl[i++];
        }
    }
    out[u] = '\0';
    return 1;
}

static int build_task_command(const Task *t, Language l, char *out, size_t n) {
    char raw[WB_MAX_ARGS][WB_INPUT] = {{0}};
    char quoted[WB_MAX_ARGS][WB_INPUT * 4] = {{0}};
    for (size_t i = 0; i < WB_MAX_ARGS; ++i) {
        const TaskArg *a = &t->args[i];
        if (a->kind == ARG_NONE) continue;
        if (!prompt_text(l, task_title(t,l), a->prompt_zh, a->prompt_en, a->default_value, a->kind,
                         raw[i], sizeof(raw[i]))) return 0;
        if (shell_quote(raw[i], quoted[i], sizeof(quoted[i]))) return 0;
    }
    return substitute_template(t->command, quoted, out, n);
}
static void execute_task(const Task*t,AppConfig*c){char cmd[MAX_COMMAND];if(!build_task_command(t,c->language,cmd,sizeof(cmd)))return;if(!confirmation_enabled(c,t->risk)||confirm_command(c->language,t,cmd))run_command(t,c->language,cmd);}

static int ci_contains(const char*h,const char*n){if(!n||!*n)return 1;size_t z=strlen(n);for(;*h;h++){size_t i=0;while(i<z&&h[i]){unsigned char a=h[i],b=n[i];if(a<128)a=(unsigned char)tolower(a);if(b<128)b=(unsigned char)tolower(b);if(a!=b)break;i++;}if(i==z)return 1;}return 0;}
static int task_matches(const Task*t,Language l,Category c,const char*q){if(c!=CAT_ALL&&t->category!=c)return 0;if(!q||!*q)return 1;return ci_contains(task_title(t,l),q)||ci_contains(task_desc(t,l),q)||ci_contains(t->keywords,q)||ci_contains(t->command,q)||ci_contains(t->id,q);}
static size_t collect_matches(Language l,Category c,const char*q,size_t*out,size_t cap){size_t n=0;for(size_t i=0;i<TASK_COUNT&&n<cap;i++)if(task_matches(&TASKS[i],l,c,q))out[n++]=i;return n;}

static size_t category_task_count(Category cat) {
    if (cat == CAT_ALL) return TASK_COUNT;
    size_t n = 0;
    for (size_t i = 0; i < TASK_COUNT; ++i) if (TASKS[i].category == cat) ++n;
    return n;
}

static void draw_task_menu(size_t sel, Category cat, const char *q, int search,
                           Language l, size_t *m, size_t mc, CommandBrowserMode mode) {
    int w=terminal_width(),h=terminal_height();if(w>1)w--;if(w>118)w=118;if(h<18)h=18;
    clear_screen();ui_hits_reset();printf("\033[?25lWorkbench %s  |  %s > %s\n",WB_VERSION,effective_workbench_title(l),category_name(cat,l));print_rule(w);
    ui_hint_line(l,mode==COMMAND_MODE_EXECUTE?"键盘：↑/k ↓/j Enter 执行 p/→ 属性；鼠标：点击按钮或结果行。":"键盘：↑/k ↓/j Enter 插入 p/→ 属性；鼠标：点击按钮或结果行。",
        mode==COMMAND_MODE_EXECUTE?"Keyboard: Up/k Down/j Enter run p/Right properties; Mouse: click buttons or result rows.":"Keyboard: Up/k Down/j Enter insert p/Right properties; Mouse: click buttons or result rows.");
    int x=1;ui_print_button(10,4,&x,tr(l,"搜索","Search"));ui_print_button(11,4,&x,tr(l,"设置","Settings"));ui_print_button(12,4,&x,tr(l,"语言","Language"));ui_print_button(13,4,&x,tr(l,"返回","Back"));putchar('\n');
    printf("%s: %s   %s: %s%s\n",tr(l,"指令种类","Category"),category_name(cat,l),tr(l,"搜索","Search"),*q?q:tr(l,"（无）","(none)"),search?tr(l," [输入中]"," [typing]"):"");
    printf("%s\n",mode==COMMAND_MODE_EXECUTE?tr(l,"提示：点击普通行只选择；点击 [执行]/[属性] 直接操作。","Tip: clicking a normal row selects it; [Run]/[Props] activates directly."):tr(l,"提示：点击 [插入] 只把命令放入 Terminal，不会自动运行。","Tip: [Insert] puts the command into Terminal without running it."));print_rule(w);
    int vis=h-12;if(vis<5)vis=5;size_t st=0;if(mc>(size_t)vis&&sel>=(size_t)vis)st=sel-(size_t)vis+1;size_t en=st+(size_t)vis;if(en>mc)en=mc;main_visible_start=st;main_visible_end=en;
    if(!mc)printf("\n  %s\n",tr(l,"没有匹配功能。再次搜索可清空关键词。","No matching task. Search again to clear the query."));
    else for(size_t r=st;r<en;r++){
        const Task*t=&TASKS[m[r]];int y=main_row_start_y+(int)(r-st);ui_hit_add(10000+(int)r,1,y,w,y);ui_hit_add(20000+(int)r,main_exec_x1,y,main_exec_x2,y);ui_hit_add(30000+(int)r,main_prop_x1,y,main_prop_x2,y);
        if(r==sel)printf("\033[7m > ");else printf("   ");print_cell(task_title(t,l),20);print_cell(category_name(t->category,l),8);print_cell(risk_name(t->risk,l),8);putchar(' ');print_cell(mode==COMMAND_MODE_EXECUTE?tr(l,"[执行]","[Run]"):tr(l,"[插入]","[Insert]"),8);putchar(' ');print_cell(tr(l,"[属性]","[Props]"),8);putchar(' ');print_cell(task_source_marker(t),7);int dc=w-67;if(dc>0){putchar(' ');print_truncated(task_desc(t,l),dc);}if(r==sel)printf("\033[0m");putchar('\n');
    }
    putchar('\n');print_rule(w);printf("%s: %zu / %zu",tr(l,"本类匹配","Matches"),mc,category_task_count(cat));if(mc)printf("   %s: %s",tr(l,"当前选择","Selected"),task_title(&TASKS[m[sel]],l));putchar('\n');fflush(stdout);
}

static int browser_activate_task(const Task*t,AppConfig*c,CommandBrowserMode mode,char*out,size_t cap){
    if(mode==COMMAND_MODE_EXECUTE){execute_task(t,c);return 0;}
    if(!out||cap==0)return 0;
    char cmd[MAX_COMMAND];if(!build_task_command(t,c->language,cmd,sizeof(cmd)))return 0;
    size_t n=strlen(cmd);if(n>=cap){show_message(c->language,"生成的命令过长。","Rendered command is too long.");return 0;}
    memcpy(out,cmd,n+1);return 1;
}

static int show_task_detail_browser(const Task*t,AppConfig*c,CommandBrowserMode mode,char*out,size_t cap){
    for(;;){Language l=c->language;int w=terminal_width();if(w>110)w=110;clear_screen();ui_hits_reset();printf("\033[?25lWorkbench %s  |  %s\n",WB_VERSION,tr(l,"属性","Properties"));print_rule(w);int x=1;ui_print_button(1,3,&x,mode==COMMAND_MODE_EXECUTE?tr(l,"执行","Run"):tr(l,"插入","Insert"));ui_print_button(2,3,&x,tr(l,"语言","Language"));ui_print_button(3,3,&x,tr(l,"返回","Back"));putchar('\n');
        printf("%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n",tr(l,"功能","Task"),task_title(t,l),tr(l,"分类","Category"),category_name(t->category,l),tr(l,"来源","Source"),task_source_name(t,l),tr(l,"风险级别","Risk class"),risk_name(t->risk,l),tr(l,mode==COMMAND_MODE_EXECUTE?"执行确认":"Terminal 行为",mode==COMMAND_MODE_EXECUTE?"Run confirmation":"Terminal behavior"),mode==COMMAND_MODE_EXECUTE?(confirmation_enabled(c,t->risk)?tr(l,"执行前询问","Ask before running"):tr(l,"直接执行","Run without asking")):tr(l,"只插入，不自动运行","Insert only; does not auto-run"),tr(l,"说明","Description"),task_desc(t,l),tr(l,"任务 ID","Task ID"),t->id);printf("\n%s\n  %s\n",tr(l,"命令模板：","Command template:"),t->command);for(size_t ai=0;ai<WB_MAX_ARGS;ai++)if(t->args[ai].kind!=ARG_NONE)printf("%s %zu: %s\n",tr(l,"参数","Parameter"),ai+1,tr(l,t->args[ai].prompt_zh,t->args[ai].prompt_en));printf("\n");print_rule(w);fflush(stdout);
        UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)return 0;if(ev.kind==UI_EVENT_KEY){int k=ev.key;if(k==27||k=='q'||k=='Q')return 0;if(k=='l'||k=='L'){c->language=c->language==LANG_ZH?LANG_EN:LANG_ZH;save_config(c);}else if(k=='\r'||k=='\n'){if(browser_activate_task(t,c,mode,out,cap))return 1;}}else if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0){int hit=ui_hit_test(&ev);if(hit==1){if(browser_activate_task(t,c,mode,out,cap))return 1;}else if(hit==2){c->language=c->language==LANG_ZH?LANG_EN:LANG_ZH;save_config(c);}else if(hit==3)return 0;}
    }
}

static void confirmation_menu(AppConfig*c){
    size_t sel=0;
    for(;;){
        Language l=c->language;const char*z[]={"特权指令","敏感指令","普通指令","返回设置"};const char*e[]={"Privileged commands","Sensitive commands","Normal commands","Back"};
        const char*v[]={c->confirm_privileged?tr(l,"询问","Ask"):tr(l,"不询问","No ask"),c->confirm_sensitive?tr(l,"询问","Ask"):tr(l,"不询问","No ask"),c->confirm_normal?tr(l,"询问","Ask"):tr(l,"不询问","No ask"),""};
        clear_screen();ui_hits_reset();printf("\033[?25lWorkbench %s | %s\n",WB_VERSION,tr(l,"执行确认策略","Run confirmation policy"));print_rule(70);ui_hint_line(l,"键盘：↑/k ↓/j Enter；Esc/q 返回。鼠标：点击任一行。","Keyboard: Up/k Down/j Enter; Esc/q back. Mouse: click any row.");putchar('\n');
        for(size_t i=0;i<4;i++){ui_hit_add(100+(int)i,1,5+(int)i,70,5+(int)i);if(i==sel)printf("\033[7m > ");else printf("   ");print_padded(l==LANG_ZH?z[i]:e[i],30);print_padded(v[i],15);if(i==sel)printf("\033[0m");putchar('\n');}fflush(stdout);
        UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)return;int activate=0;
        if(ev.kind==UI_EVENT_KEY){int k=ev.key;if(k==27||k=='q'||k=='Q')return;if(k==KEY_UP||k=='k'||k=='K')sel=sel?sel-1:3;else if(k==KEY_DOWN||k=='j'||k=='J')sel=(sel+1)%4;else if(k=='\r'||k=='\n')activate=1;}
        else if(ev.kind==UI_EVENT_WHEEL_UP)sel=sel?sel-1:3;else if(ev.kind==UI_EVENT_WHEEL_DOWN)sel=(sel+1)%4;
        else if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0){int hit=ui_hit_test(&ev);if(hit>=100&&hit<104){sel=(size_t)(hit-100);activate=1;}}
        if(!activate) continue;
        if(sel==3) return;
        if(sel==0&&c->confirm_privileged){if(!confirm_line(l,"关闭特权指令确认后，高风险操作会直接执行。确认关闭？","High-risk operations will run directly if privileged confirmation is disabled. Continue?"))continue;c->confirm_privileged=0;}
        else if(sel==0) c->confirm_privileged=1;
        else if(sel==1) c->confirm_sensitive=!c->confirm_sensitive;
        else c->confirm_normal=!c->confirm_normal;
        save_config(c);
    }
}
static void linux_profile_menu(AppConfig*c){
    size_t sel=0;const LinuxProfile profiles[]={PROFILE_GENERIC,PROFILE_CENTOS};
    for(;;){
        Language l=c->language;DetectedLinux d;detect_linux(&d);LinuxProfile effective=effective_linux_profile(c,&d);
        clear_screen();ui_hits_reset();printf("\033[?25lWorkbench %s | %s\n",WB_VERSION,tr(l,"Linux 版本","Linux version"));print_rule(78);ui_hint_line(l,"键盘：↑/k ↓/j Enter；Esc/q 返回。鼠标：点击选项。","Keyboard: Up/k Down/j Enter; Esc/q back. Mouse: click an option.");putchar('\n');
        for(size_t i=0;i<3;i++){const char*label=i<2?linux_profile_label(profiles[i],l):tr(l,"返回设置","Back to settings");const char*value=(i<2&&c->linux_profile==profiles[i])?tr(l,"当前","Current"):"";ui_hit_add(100+(int)i,1,5+(int)i,78,5+(int)i);if(i==sel)printf("\033[7m > ");else printf("   ");print_padded(label,28);print_padded(value,12);if(i==sel)printf("\033[0m");putchar('\n');}
        putchar('\n');print_rule(78);printf("%s: %s%s%s\n",tr(l,"检测结果","Detected"),d.pretty_name[0]?d.pretty_name:d.id,d.version[0]?"  ":"",d.version[0]?d.version:"");printf("%s: %s\n",tr(l,"当前使用","Active profile"),linux_profile_label(effective,l));printf("%s\n",tr(l,"检测结果仅供参考，不会自动切换。只有手动选择 CentOS 才会启用 CentOS 指令。","Detection is advisory only and never switches profiles automatically. CentOS actions are enabled only after manual selection."));fflush(stdout);
        UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)return;int activate=0;
        if(ev.kind==UI_EVENT_KEY){int k=ev.key;if(k==27||k=='q'||k=='Q')return;if(k==KEY_UP||k=='k'||k=='K')sel=sel?sel-1:2;else if(k==KEY_DOWN||k=='j'||k=='J')sel=(sel+1)%3;else if(k=='\r'||k=='\n')activate=1;}
        else if(ev.kind==UI_EVENT_WHEEL_UP)sel=sel?sel-1:2;else if(ev.kind==UI_EVENT_WHEEL_DOWN)sel=(sel+1)%3;
        else if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0){int hit=ui_hit_test(&ev);if(hit>=100&&hit<103){sel=(size_t)(hit-100);activate=1;}}
        if(!activate) continue;
        if(sel==2) return;
        c->linux_profile=profiles[sel];save_config(c);
        if(rebuild_from_config(c)) show_message(l,"重建指令集失败。","Failed to rebuild command catalogue.");
    }
}

static int clamp_int(int v,int lo,int hi){if(v<lo)return lo;if(v>hi)return hi;return v;}
static void number_setting_menu(AppConfig*c,int*value,int minv,int maxv,int step,int big,const char*zh_title,const char*en_title,const char*zh_desc,const char*en_desc){
    for(;;){
        Language l=c->language;int w=terminal_width();if(w>90)w=90;clear_screen();ui_hits_reset();
        printf("\033[?25lWorkbench %s | %s\n",WB_VERSION,tr(l,zh_title,en_title));print_rule(w);
        printf("%s\n\n",tr(l,zh_desc,en_desc));
        printf("%s: %d\n\n",tr(l,"当前值","Current value"),*value);
        int x=2,y=7;char a[32],b[32],d[32],e[32];snprintf(a,sizeof(a),"-%d",big);snprintf(b,sizeof(b),"-%d",step);snprintf(d,sizeof(d),"+%d",step);snprintf(e,sizeof(e),"+%d",big);
        ui_print_button(1,y,&x,a);ui_print_button(2,y,&x,b);ui_print_button(3,y,&x,d);ui_print_button(4,y,&x,e);ui_print_button(5,y,&x,tr(l,"返回","Back"));putchar('\n');
        ui_hint_line(l,"键盘：←/h 减少，→/l 增加，↓/j 大幅减少，↑/k 大幅增加，Enter/Esc/q 返回。","Keyboard: Left/h decrease, Right/l increase, Down/j large decrease, Up/k large increase, Enter/Esc/q back.");
        fflush(stdout);UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)return;int delta=0;
        if(ev.kind==UI_EVENT_KEY){int k=ev.key;if(k==27||k=='q'||k=='Q'||k=='\r'||k=='\n')return;if(k==KEY_LEFT||k=='h'||k=='H')delta=-step;else if(k==KEY_RIGHT||k=='l'||k=='L')delta=step;else if(k==KEY_DOWN||k=='j'||k=='J')delta=-big;else if(k==KEY_UP||k=='k'||k=='K')delta=big;}
        else if(ev.kind==UI_EVENT_WHEEL_UP)delta=step;else if(ev.kind==UI_EVENT_WHEEL_DOWN)delta=-step;
        else if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0){int hit=ui_hit_test(&ev);if(hit==1)delta=-big;else if(hit==2)delta=-step;else if(hit==3)delta=step;else if(hit==4)delta=big;else if(hit==5)return;}
        if(delta){int nv=clamp_int(*value+delta,minv,maxv);if(nv!=*value){*value=nv;save_config(c);}}
    }
}

static void settings_menu(AppConfig*c){
    size_t sel=0;
    for(;;){
        Language l=c->language;int auto_on=autostart_enabled();DetectedLinux d;detect_linux(&d);LinuxProfile effective=effective_linux_profile(c,&d);
        const char*z[]={"界面语言","Linux 版本","执行确认策略","虚拟键盘","快捷键提示","终端日志行数","终端上滑加载","重新打开终端时清屏","终端登录自启动","返回主菜单"};
        const char*e[]={"Interface language","Linux version","Run confirmation policy","Virtual keyboard","Shortcut hints","Terminal log lines","Terminal history load","Clear terminal on reopen","Terminal login autostart","Back to main"};
        char pol[32],profile[64],logv[32],loadv[32];
        snprintf(pol,sizeof(pol),l==LANG_ZH?"%d/3 询问":"%d/3 ask",c->confirm_normal+c->confirm_sensitive+c->confirm_privileged);snprintf(profile,sizeof(profile),"%s",linux_profile_label(c->linux_profile,l));snprintf(logv,sizeof(logv),"%d",c->terminal_log_lines);snprintf(loadv,sizeof(loadv),"%d",c->terminal_load_lines);
        const char*v[]={l==LANG_ZH?"简体中文":"English",profile,pol,c->virtual_keyboard?tr(l,"已启用","Enabled"):tr(l,"已关闭","Disabled"),c->shortcut_hints?tr(l,"已启用","Enabled"):tr(l,"已关闭","Disabled"),logv,loadv,c->terminal_clear_on_open?tr(l,"已启用","Enabled"):tr(l,"已关闭","Disabled"),auto_on?tr(l,"已启用","Enabled"):tr(l,"未启用","Disabled"),""};
        clear_screen();ui_hits_reset();printf("\033[?25lWorkbench %s | %s\n",WB_VERSION,tr(l,"设置","Settings"));print_rule(82);ui_hint_line(l,"键盘：↑/k ↓/j Enter；Esc/q 返回。鼠标：点击设置项。","Keyboard: Up/k Down/j Enter; Esc/q back. Mouse: click a setting.");putchar('\n');
        for(size_t i=0;i<10;i++){ui_hit_add(100+(int)i,1,5+(int)i,82,5+(int)i);if(i==sel)printf("\033[7m > ");else printf("   ");print_padded(l==LANG_ZH?z[i]:e[i],28);print_padded(v[i],24);if(i==sel)printf("\033[0m");putchar('\n');}
        putchar('\n');print_rule(82);
        if(sel==1){printf("%s: %s\n",tr(l,"检测结果（仅供参考）","Detected (reference only)"),d.pretty_name[0]?d.pretty_name:d.id);printf("%s: %s\n",tr(l,"当前使用","Active"),linux_profile_label(effective,l));printf("%s\n",tr(l,"系统检测不会自动改变 Linux 版本设置。","System detection never changes the Linux version setting automatically."));}
        else if(sel==3)printf("%s\n",tr(l,"关闭时不显示屏幕虚拟键盘；实体键盘输入始终可用。","When disabled, the on-screen keyboard is hidden; physical keyboard input always remains available."));
        else if(sel==4)printf("%s\n",tr(l,"控制 Workbench 各页面的键盘/鼠标快捷操作提示；不影响快捷键本身。","Controls keyboard/mouse shortcut legends across Workbench; shortcuts themselves remain active."));
        else if(sel==5)printf("%s\n",tr(l,"当前 Workbench 会话最多保留的终端输出行数；默认 1000 行，不写入磁盘。","Maximum terminal output lines kept in the current Workbench session; default 1000 and never written to disk."));
        else if(sel==6)printf("%s\n",tr(l,"每次向上/向下翻阅终端历史加载的行数；默认 50 行。","Number of terminal history lines loaded per older/newer action; default 50."));
        else if(sel==7)printf("%s\n",tr(l,"默认关闭：返回首页后再次打开终端仍保留上次屏幕与短期日志。","Disabled by default: reopening Terminal keeps the previous screen and short-lived history."));
        else if(sel==8)printf("%s: %s\n",tr(l,"将修改","Shell config"),detect_shell_rc()?detect_shell_rc():tr(l,"无法检测","Unknown"));
        fflush(stdout);UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)return;int activate=0;
        if(ev.kind==UI_EVENT_KEY){int k=ev.key;if(k==27||k=='q'||k=='Q')return;if(k==KEY_UP||k=='k'||k=='K')sel=sel?sel-1:9;else if(k==KEY_DOWN||k=='j'||k=='J')sel=(sel+1)%10;else if(k=='l'||k=='L'){c->language=c->language==LANG_ZH?LANG_EN:LANG_ZH;save_config(c);}else if(k=='\r'||k=='\n')activate=1;}
        else if(ev.kind==UI_EVENT_WHEEL_UP)sel=sel?sel-1:9;else if(ev.kind==UI_EVENT_WHEEL_DOWN)sel=(sel+1)%10;
        else if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0){int hit=ui_hit_test(&ev);if(hit>=100&&hit<110){sel=(size_t)(hit-100);activate=1;}}
        if(!activate)continue;
        if(sel==0){c->language=c->language==LANG_ZH?LANG_EN:LANG_ZH;save_config(c);}else if(sel==1)linux_profile_menu(c);else if(sel==2)confirmation_menu(c);else if(sel==3){c->virtual_keyboard=!c->virtual_keyboard;save_config(c);}else if(sel==4){c->shortcut_hints=!c->shortcut_hints;save_config(c);}else if(sel==5)number_setting_menu(c,&c->terminal_log_lines,100,10000,100,1000,"终端日志行数","Terminal log lines","设置当前 Workbench 会话最多保留多少行终端输出。","Set how many terminal output lines are retained in the current Workbench session.");else if(sel==6)number_setting_menu(c,&c->terminal_load_lines,10,500,10,50,"终端上滑加载","Terminal history load","设置每次查看更早/更新终端历史时移动多少行。","Set how many lines each older/newer terminal history action moves.");else if(sel==7){c->terminal_clear_on_open=!c->terminal_clear_on_open;save_config(c);}else if(sel==8){if(!auto_on){if(confirm_line(l,"启用后，每次进入交互式终端会自动打开 Workbench。确认启用？","Workbench will open automatically for interactive terminals. Enable?")){if(enable_autostart())show_message(l,"启用失败。","Enable failed.");}}else if(confirm_line(l,"确认关闭终端登录自启动？","Disable terminal login autostart?")){if(disable_autostart())show_message(l,"关闭失败。","Disable failed.");}}else return;
    }
}

typedef enum {
    TERM_ACTION_RESUME=0, TERM_ACTION_COMMANDS, TERM_ACTION_INPUT, TERM_ACTION_RUN,
    TERM_ACTION_OLDER, TERM_ACTION_NEWER, TERM_ACTION_CLEAR, TERM_ACTION_END_RESTART,
    TERM_ACTION_BACK
} TerminalAction;

static const char *terminal_shell_path(void){const char*s=getenv("SHELL");return s&&*s&&access(s,X_OK)==0?s:"/bin/sh";}
static int terminal_prepare(AppConfig*c){
    if(!g_terminal_session_initialized){if(wb_terminal_init(&g_terminal_session,(size_t)c->terminal_log_lines))return-1;g_terminal_session_initialized=1;}
    if(wb_terminal_set_limit(&g_terminal_session,(size_t)c->terminal_log_lines))return-1;
    return 0;
}
static void terminal_drain_to_idle(int quiet_ms,int max_ms){
    int fd=wb_terminal_master_fd(&g_terminal_session);
    if(fd<0||!wb_terminal_is_running(&g_terminal_session))return;
    int elapsed=0;
    while(elapsed<max_ms){
        struct pollfd pfd={fd,POLLIN|POLLHUP|POLLERR,0};
        int wait=quiet_ms;
        if(wait>max_ms-elapsed)wait=max_ms-elapsed;
        int rc=poll(&pfd,1,wait);
        elapsed+=wait;
        if(rc<=0)break;
        wb_terminal_pump(&g_terminal_session);
    }
}
static int terminal_start_shell(AppConfig*c){
    if(terminal_prepare(c))return-1;
    if(wb_terminal_is_running(&g_terminal_session))return 0;
    char err[256]={0};int rows=terminal_height()-6;if(rows<8)rows=8;
    if(wb_terminal_start(&g_terminal_session,NULL,terminal_width(),rows,err,sizeof(err))){show_message(c->language,"无法启动 Shell。","Unable to start the shell.");return-1;}
    return 0;
}
static size_t terminal_total_lines(void){size_t n=wb_terminal_line_count(&g_terminal_session);if(*wb_terminal_current_line(&g_terminal_session))n++;return n;}
static const char *terminal_line_by_display_index(size_t i){size_t n=wb_terminal_line_count(&g_terminal_session);if(i<n)return wb_terminal_line_at(&g_terminal_session,i);if(i==n&&*wb_terminal_current_line(&g_terminal_session))return wb_terminal_current_line(&g_terminal_session);return "";}
static void terminal_adjust_scroll(size_t*offset,int direction,const AppConfig*c,int body_lines){
    size_t total=terminal_total_lines(),floor=wb_terminal_screen_floor(&g_terminal_session);if(floor>total)floor=total;size_t live_start=total>(size_t)body_lines?total-(size_t)body_lines:0;if(live_start<floor)live_start=floor;size_t step=(size_t)c->terminal_load_lines;
    if(direction>0){size_t nv=*offset+step;*offset=nv>live_start?live_start:nv;}else *offset=*offset>step?*offset-step:0;
}
static void draw_terminal(AppConfig*c,size_t scroll_offset){
    Language l=c->language;int w=terminal_width();if(w>1)w--;if(w>118)w=118;int h=terminal_height();if(h<14)h=14;int body=h-7;if(body<5)body=5;
    wb_terminal_resize(&g_terminal_session,w,body);
    clear_screen();ui_hits_reset();printf("\033[?25lWorkbench Terminal %s | %s\n",WB_VERSION,tr(l,"终端","Terminal"));print_rule(w);
    int x=1;char oldb[32],newb[32];snprintf(oldb,sizeof(oldb),l==LANG_ZH?"上翻%d":"Older %d",c->terminal_load_lines);snprintf(newb,sizeof(newb),l==LANG_ZH?"下翻%d":"Newer %d",c->terminal_load_lines);
    ui_print_button(TERM_ACTION_BACK,3,&x,tr(l,"返回","Back"));ui_print_button(TERM_ACTION_COMMANDS,3,&x,tr(l,"指令集","Commands"));ui_print_button(TERM_ACTION_INPUT,3,&x,tr(l,"输入","Input"));ui_print_button(TERM_ACTION_RUN,3,&x,tr(l,"运行","Run"));ui_print_button(TERM_ACTION_OLDER,3,&x,oldb);ui_print_button(TERM_ACTION_NEWER,3,&x,newb);ui_print_button(TERM_ACTION_CLEAR,3,&x,tr(l,"清屏","Clear"));ui_print_button(TERM_ACTION_END_RESTART,3,&x,wb_terminal_is_running(&g_terminal_session)?tr(l,"结束 Shell","End Shell"):tr(l,"重启 Shell","Restart Shell"));putchar('\n');
    printf("%s: %s  |  Shell: %s  |  %s: %zu/%d  |  %s: %s\n",tr(l,"状态","Status"),wb_terminal_is_running(&g_terminal_session)?tr(l,"运行中","Running"):tr(l,"已结束","Ended"),terminal_shell_path(),tr(l,"短期日志","History"),wb_terminal_line_count(&g_terminal_session),c->terminal_log_lines,tr(l,"视图","View"),scroll_offset?tr(l,"历史","History"):tr(l,"实时","Live"));
    ui_hint_line(l,"键盘直接发送给 Shell；Ctrl+] 打开 Workbench 终端控制。鼠标滚轮按设置的行数翻阅历史。","Keyboard input goes to the shell; Ctrl+] opens Workbench terminal controls. Mouse wheel moves history by the configured line count.");
    print_rule(w);
    size_t total=terminal_total_lines(),floor=wb_terminal_screen_floor(&g_terminal_session);if(floor>total)floor=total;size_t live_start=total>(size_t)body?total-(size_t)body:0;if(live_start<floor)live_start=floor;size_t start=scroll_offset>live_start?0:live_start-scroll_offset;size_t end=scroll_offset?start+(size_t)body:total;if(end>total)end=total;
    int printed=0;for(size_t i=start;i<end&&printed<body;i++,printed++){const char*line=terminal_line_by_display_index(i);print_truncated(line,w);putchar('\n');}
    while(printed++<body)putchar('\n');
    fflush(stdout);
}
static int terminal_poll_ui(int timeout_ms,UiEvent*out,int*changed){
    if(changed)*changed=0;
    if(ui_pending_valid){if(out)*out=ui_read_event_ready();return 1;}
    struct pollfd fds[2];nfds_t n=1;fds[0]=(struct pollfd){STDIN_FILENO,POLLIN,0};int tfd=wb_terminal_is_running(&g_terminal_session)?wb_terminal_master_fd(&g_terminal_session):-1;if(tfd>=0)fds[n++]=(struct pollfd){tfd,POLLIN|POLLHUP|POLLERR,0};
    int rc=poll(fds,n,timeout_ms);if(rc<0){if(errno==EINTR)return 0;return-1;}if(n>1&&fds[1].revents){int got=wb_terminal_pump(&g_terminal_session);if(changed&&got>=0)*changed=1;}if(fds[0].revents&POLLIN){if(out)*out=ui_read_event_ready();return 1;}return 0;
}
static int terminal_send_event_key(const UiEvent*ev){
    if(!ev||ev->kind!=UI_EVENT_KEY||!wb_terminal_is_running(&g_terminal_session))return 0;
    const char*seq=NULL;char c=0;
    if(ev->key==KEY_UP)seq="\033[A";else if(ev->key==KEY_DOWN)seq="\033[B";else if(ev->key==KEY_RIGHT)seq="\033[C";else if(ev->key==KEY_LEFT)seq="\033[D";
    if(seq)return wb_terminal_send(&g_terminal_session,seq,strlen(seq))>=0;
    c=(char)ev->key;
    return wb_terminal_send(&g_terminal_session,&c,1)>=0;
}
static int terminal_control_menu(AppConfig*c){
    size_t sel=0;for(;;){Language l=c->language;const char*z[]={"返回终端","指令集","输入一行","运行当前命令","查看更早日志","查看更新日志","清屏（保留日志）","结束/重启 Shell","返回桌面"};const char*e[]={"Resume terminal","Command Sets","Input a line","Run current command","Older history","Newer history","Clear screen (keep history)","End/Restart Shell","Back to desktop"};
        int w=terminal_width();if(w>90)w=90;clear_screen();ui_hits_reset();printf("\033[?25lWorkbench %s | %s\n",WB_VERSION,tr(l,"终端控制","Terminal controls"));print_rule(w);ui_hint_line(l,"键盘：↑/↓ 选择，Enter 执行；b 返回桌面，Esc/q 返回终端。鼠标可点击。","Keyboard: Up/Down select, Enter activate; b back to desktop, Esc/q resumes terminal. Mouse can click.");putchar('\n');
        for(size_t i=0;i<9;i++){ui_hit_add(100+(int)i,1,5+(int)i,w,5+(int)i);if(i==sel)printf("\033[7m > ");else printf("   ");print_padded(l==LANG_ZH?z[i]:e[i],38);if(i==sel)printf("\033[0m");putchar('\n');}fflush(stdout);UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)return TERM_ACTION_BACK;int activate=0;
        if(ev.kind==UI_EVENT_KEY){int k=ev.key;if(k==27||k=='q'||k=='Q')return TERM_ACTION_RESUME;if(k=='b'||k=='B')return TERM_ACTION_BACK;if(k==KEY_UP||k=='k'||k=='K')sel=sel?sel-1:8;else if(k==KEY_DOWN||k=='j'||k=='J')sel=(sel+1)%9;else if(k=='\r'||k=='\n')activate=1;}
        else if(ev.kind==UI_EVENT_WHEEL_UP)sel=sel?sel-1:8;else if(ev.kind==UI_EVENT_WHEEL_DOWN)sel=(sel+1)%9;else if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0){int hit=ui_hit_test(&ev);if(hit>=100&&hit<109){sel=(size_t)(hit-100);activate=1;}}
        if(activate){static const int map[]={TERM_ACTION_RESUME,TERM_ACTION_COMMANDS,TERM_ACTION_INPUT,TERM_ACTION_RUN,TERM_ACTION_OLDER,TERM_ACTION_NEWER,TERM_ACTION_CLEAR,TERM_ACTION_END_RESTART,TERM_ACTION_BACK};return map[sel];}
    }
}
static void terminal_input_line(AppConfig*c){char line[WB_INPUT];if(!wb_terminal_is_running(&g_terminal_session)){show_message(c->language,"Shell 已结束，请先重启 Shell。","The shell has ended; restart it first.");return;}if(ui_input_dialog(c->language,tr(c->language,"终端输入","Terminal input"),tr(c->language,"输入要发送给 Shell 的一行文字。","Enter one line to send to the shell."),"",line,sizeof(line),1)){wb_terminal_send(&g_terminal_session,line,strlen(line));wb_terminal_send(&g_terminal_session,"\n",1);}}
static int terminal_handle_action(AppConfig*c,int action,size_t*scroll){
    if(action==TERM_ACTION_RESUME)return 0;
    if(action==TERM_ACTION_BACK)return 1;
    if(action==TERM_ACTION_COMMANDS){
        char cmd[MAX_COMMAND]="";
        if(command_sets_menu(c,COMMAND_MODE_INSERT,cmd,sizeof(cmd))&&*cmd){
            if(wb_terminal_is_running(&g_terminal_session))wb_terminal_send(&g_terminal_session,cmd,strlen(cmd));
            else show_message(c->language,"Shell 已结束，请先重启 Shell。","The shell has ended; restart it first.");
        }
        return 0;
    }
    if(action==TERM_ACTION_INPUT){terminal_input_line(c);return 0;}
    if(action==TERM_ACTION_RUN){if(wb_terminal_is_running(&g_terminal_session))wb_terminal_send(&g_terminal_session,"\n",1);return 0;}
    if(action==TERM_ACTION_OLDER){terminal_adjust_scroll(scroll,1,c,terminal_height()-7);return 0;}
    if(action==TERM_ACTION_NEWER){terminal_adjust_scroll(scroll,-1,c,terminal_height()-7);return 0;}
    if(action==TERM_ACTION_CLEAR){wb_terminal_clear_screen(&g_terminal_session);*scroll=0;return 0;}
    if(action==TERM_ACTION_END_RESTART){
        if(wb_terminal_is_running(&g_terminal_session)){
            if(confirm_line(c->language,"结束当前 Shell？终端短期日志会保留。","End the current shell? Short-lived terminal history will be kept.")){
                wb_terminal_end(&g_terminal_session);
                const unsigned char mark[]="\n--- Shell ended ---\n";
                wb_terminal_feed(&g_terminal_session,mark,sizeof(mark)-1);
            }
        }else{
            const unsigned char mark[]="\n--- Shell restarted ---\n";
            wb_terminal_feed(&g_terminal_session,mark,sizeof(mark)-1);
            terminal_start_shell(c);
        }
        return 0;
    }
    return 0;
}
static void terminal_menu(AppConfig*c){
    if(terminal_prepare(c)){show_message(c->language,"终端初始化失败。","Terminal initialization failed.");return;}
    if(c->terminal_clear_on_open){terminal_drain_to_idle(20,120);wb_terminal_clear_screen(&g_terminal_session);}
    if(!wb_terminal_is_running(&g_terminal_session)&&!g_terminal_session.exited)terminal_start_shell(c);
    size_t scroll=0;
    int dirty=1;
    terminal_passthrough_signals(1);
    for(;;){if(dirty){draw_terminal(c,scroll);dirty=0;}UiEvent ev={0};int changed=0,has=terminal_poll_ui(120,&ev,&changed);if(has<0)break;if(changed)dirty=1;if(!has)continue;
        if(ev.kind==UI_EVENT_WHEEL_UP){terminal_adjust_scroll(&scroll,1,c,terminal_height()-7);dirty=1;continue;}if(ev.kind==UI_EVENT_WHEEL_DOWN){terminal_adjust_scroll(&scroll,-1,c,terminal_height()-7);dirty=1;continue;}
        if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0){int hit=ui_hit_test(&ev);if(hit>=TERM_ACTION_RESUME&&hit<=TERM_ACTION_BACK){if(terminal_handle_action(c,hit,&scroll))break;dirty=1;}continue;}
        if(ev.kind==UI_EVENT_KEY){if(ev.key==29){int action=terminal_control_menu(c);if(terminal_handle_action(c,action,&scroll))break;dirty=1;continue;}if(terminal_send_event_key(&ev)){scroll=0;dirty=1;}}
    }
    terminal_passthrough_signals(0);
}

#include "modules/files-manager/files_ui.inc"

static const Category CATEGORY_MENU_ORDER[] = {
    CAT_SYSTEM, CAT_FILES, CAT_TEXT, CAT_PROCESS, CAT_NETWORK,
    CAT_STORAGE, CAT_PERMISSION, CAT_ARCHIVE, CAT_USER,
    CAT_PACKAGE, CAT_SERVICE, CAT_FIREWALL, CAT_SELINUX, CAT_ALL
};

static size_t collect_visible_categories(Category*out,size_t cap){
    size_t n=0;
    for(size_t i=0;i<ARRAY_LEN(CATEGORY_MENU_ORDER)&&n<cap;i++){
        Category c=CATEGORY_MENU_ORDER[i];
        if(c==CAT_ALL||category_task_count(c)>0)out[n++]=c;
    }
    return n;
}
static const char *category_desc(Category c,Language l){
    switch(c){
        case CAT_SYSTEM:return tr(l,"内核、CPU、内存、时间、环境与系统信息","Kernel, CPU, memory, time, environment and system info");
        case CAT_FILES:return tr(l,"目录、文件、查找、复制、移动、删除与链接","Directories, files, find, copy, move, delete and links");
        case CAT_TEXT:return tr(l,"搜索、排序、去重、比较、统计与文本查看","Search, sort, deduplicate, compare, count and inspect text");
        case CAT_PROCESS:return tr(l,"进程查看、查找、终止与优先级","Inspect, find, terminate and reprioritize processes");
        case CAT_NETWORK:return tr(l,"地址、路由、端口、DNS、连通、HTTP、SSH 与 NetworkManager","Addresses, routes, ports, DNS, connectivity, HTTP, SSH and NetworkManager");
        case CAT_STORAGE:return tr(l,"空间、inode、设备、挂载与同步","Space, inodes, devices, mounts and sync");
        case CAT_PERMISSION:return tr(l,"权限、所有者、用户组与 umask","Permissions, ownership, groups and umask");
        case CAT_ARCHIVE:return tr(l,"tar、gzip、xz 打包与解压","tar, gzip and xz archive/compression operations");
        case CAT_USER:return tr(l,"当前用户、用户组、登录会话与密码","Current user, groups, login sessions and password");
        case CAT_PACKAGE:return tr(l,"DNF、RPM 软件包、仓库、事务与软件包组","DNF/RPM packages, repositories, transactions and groups");
        case CAT_SERVICE:return tr(l,"systemd 服务控制与 journal 日志","systemd service control and journal logs");
        case CAT_FIREWALL:return tr(l,"firewalld 区域、服务、端口与永久配置","firewalld zones, services, ports and permanent configuration");
        case CAT_SELINUX:return tr(l,"SELinux 模式、标签、Boolean 与策略映射","SELinux modes, labels, booleans and policy mappings");
        case CAT_ALL:return tr(l,"浏览和搜索当前 Linux 配置可用的全部操作","Browse and search all actions available for the current Linux profile");
        default:return "";
    }
}

static void draw_desktop(size_t sel, Language l) {
    int w=terminal_width();if(w>1)w--;if(w>100)w=100;
    const size_t total=5;
    clear_screen();printf("\033[?25lWorkbench %s  |  %s\n",WB_VERSION,tr(l,"桌面","Desktop"));
    print_rule(w);
    ui_hint_line(l,"操作：↑/k 上移  ↓/j 下移  Enter 打开/执行  s 设置  l 语言  q/Esc 退出  鼠标可点击","Controls: Up/k previous  Down/j next  Enter open/run  s settings  l language  q/Esc quit  Mouse click");
    printf("%s\n",tr(l,"Workbench Files、内嵌终端和指令集都是独立应用；Linux 系统指令集位于“指令集”内。","Workbench Files, the embedded Terminal, and Command Sets are independent apps; Linux system commands live inside Command Sets."));
    print_rule(w);
    for(size_t i=0;i<total;i++){
        const char *title,*desc;char descbuf[192];
        if(i==0){title="Workbench Files";desc=tr(l,"本地文件浏览、查看、复制、移动、重命名、删除与权限管理","Local browse, view, copy, move, rename, delete and permissions");}
        else if(i==1){title=tr(l,"终端","Terminal");desc=tr(l,"Workbench 内嵌 PTY Shell、短期日志和指令集插入","Embedded PTY shell, short-lived history and Command Set insertion");}
        else if(i==2){title=tr(l,"指令集","Command Sets");snprintf(descbuf,sizeof(descbuf),l==LANG_ZH?"Linux 系统预制 + 用户自定义 · %zu 个系统操作":"Linux system preset + user sets · %zu system actions",TASK_COUNT);desc=descbuf;}
        else if(i==3){title=tr(l,"设置","Settings");desc=tr(l,"Linux 版本、语言、双输入、终端日志与执行确认","Linux profile, language, dual input, terminal history and confirmations");}
        else{title=tr(l,"退出 Workbench","Exit Workbench");desc=tr(l,"关闭当前 Workbench","Close the current Workbench");}
        if(i==sel)printf("\033[7m > ");else printf("   ");print_cell(title,30);putchar(' ');print_truncated(desc,w-35);if(i==sel)printf("\033[0m");putchar('\n');
    }
    putchar('\n');print_rule(w);printf("%s: 3   %s: %zu   %s: %zu   %s: %zu\n",tr(l,"应用","Apps"),tr(l,"指令集","Command sets"),g_command_sets_initialized?g_command_sets.count:1,tr(l,"系统模块","System modules"),module_count(),tr(l,"系统功能","System actions"),total_module_actions());fflush(stdout);
}

static void draw_category_menu(size_t sel, Language l, const Category *cats,size_t cat_count) {
    int w=terminal_width();if(w>1)w--;if(w>100)w=100;clear_screen();ui_hits_reset();printf("\033[?25lWorkbench %s  |  %s > %s\n",WB_VERSION,tr(l,"桌面","Desktop"),effective_workbench_title(l));print_rule(w);
    ui_hint_line(l,"键盘：↑/k ↓/j Enter；←/Esc/q 返回。鼠标：点击按钮或分类。","Keyboard: Up/k Down/j Enter; Left/Esc/q back. Mouse: click buttons or a category.");int x=1;ui_print_button(10,4,&x,tr(l,"设置","Settings"));ui_print_button(11,4,&x,tr(l,"语言","Language"));ui_print_button(12,4,&x,tr(l,"返回","Back"));putchar('\n');printf("%s\n",tr(l,"只显示当前 Linux 版本配置中有可用操作的分类。","Only categories with actions in the current Linux profile are shown."));print_rule(w);
    for(size_t i=0;i<cat_count;i++){Category c=cats[i];char count[32];snprintf(count,sizeof(count),"%zu",category_task_count(c));ui_hit_add(100+(int)i,1,7+(int)i,w,7+(int)i);if(i==sel)printf("\033[7m > ");else printf("   ");print_cell(category_name(c,l),24);printf("(%s)  ",count);print_truncated(category_desc(c,l),w-34);if(i==sel)printf("\033[0m");putchar('\n');}
    putchar('\n');print_rule(w);printf("%s: %s\n",tr(l,"当前选择","Selected"),category_name(cats[sel],l));fflush(stdout);
}

static int task_menu_browser(AppConfig *c, Category cat,CommandBrowserMode mode,char*out,size_t cap) {
    char q[MAX_QUERY]="";size_t sel=0,m[WB_MAX_EFFECTIVE_TASKS];
    for(;;){size_t mc=collect_matches(c->language,cat,q,m,ARRAY_LEN(m));if(!mc)sel=0;else if(sel>=mc)sel=mc-1;draw_task_menu(sel,cat,q,0,c->language,m,mc,mode);UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)return 0;int action=0;
        if(ev.kind==UI_EVENT_KEY){int k=ev.key;if(k==27||k=='q'||k=='Q'||k==KEY_LEFT)return 0;if(k==KEY_UP||k=='k'||k=='K'){if(mc)sel=sel?sel-1:mc-1;}else if(k==KEY_DOWN||k=='j'||k=='J'){if(mc)sel=(sel+1)%mc;}else if(k=='/')action=10;else if(k=='s'||k=='S')action=11;else if(k=='l'||k=='L')action=12;else if((k=='p'||k=='P'||k==KEY_RIGHT)&&mc){if(show_task_detail_browser(&TASKS[m[sel]],c,mode,out,cap))return 1;}else if((k=='\r'||k=='\n')&&mc){if(browser_activate_task(&TASKS[m[sel]],c,mode,out,cap))return 1;}}
        else if(ev.kind==UI_EVENT_WHEEL_UP){if(mc)sel=sel?sel-1:mc-1;}else if(ev.kind==UI_EVENT_WHEEL_DOWN){if(mc)sel=(sel+1)%mc;}
        else if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0){int hit=ui_hit_test(&ev);if(hit>=30000){size_t row=(size_t)(hit-30000);if(row<mc){sel=row;if(show_task_detail_browser(&TASKS[m[sel]],c,mode,out,cap))return 1;}}else if(hit>=20000){size_t row=(size_t)(hit-20000);if(row<mc){sel=row;if(browser_activate_task(&TASKS[m[sel]],c,mode,out,cap))return 1;}}else if(hit>=10000){size_t row=(size_t)(hit-10000);if(row<mc)sel=row;}else action=hit;}
        if(action==10){char nq[MAX_QUERY]="";if(ui_input_dialog(c->language,tr(c->language,"搜索","Search"),tr(c->language,"输入关键词；留空确定可清除搜索。","Enter keywords; accept empty input to clear search."),NULL,nq,sizeof(nq),1)){strcpy(q,nq);sel=0;}}
        else if(action==11)settings_menu(c);else if(action==12){c->language=c->language==LANG_ZH?LANG_EN:LANG_ZH;save_config(c);sel=0;}else if(action==13)return 0;
    }
}

static int module_command_set_browser(AppConfig *c, const Module *module,CommandBrowserMode mode,char*out,size_t cap) {
    (void)module;size_t sel=0;
    for(;;){Category cats[CAT_COUNT];size_t cat_count=collect_visible_categories(cats,ARRAY_LEN(cats));if(!cat_count)break;if(sel>=cat_count)sel=0;draw_category_menu(sel,c->language,cats,cat_count);UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)break;int action=0;
        if(ev.kind==UI_EVENT_KEY){int k=ev.key;if(k==27||k=='q'||k=='Q'||k==KEY_LEFT)break;if(k==KEY_UP||k=='k'||k=='K')sel=sel?sel-1:cat_count-1;else if(k==KEY_DOWN||k=='j'||k=='J')sel=(sel+1)%cat_count;else if(k=='s'||k=='S')action=10;else if(k=='l'||k=='L')action=11;else if(k=='\r'||k=='\n'){if(task_menu_browser(c,cats[sel],mode,out,cap))return 1;}}
        else if(ev.kind==UI_EVENT_WHEEL_UP)sel=sel?sel-1:cat_count-1;else if(ev.kind==UI_EVENT_WHEEL_DOWN)sel=(sel+1)%cat_count;
        else if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0){int hit=ui_hit_test(&ev);if(hit>=100){size_t row=(size_t)(hit-100);if(row<cat_count){sel=row;if(task_menu_browser(c,cats[sel],mode,out,cap))return 1;}}else action=hit;}
        if(action==10)settings_menu(c);else if(action==11){c->language=c->language==LANG_ZH?LANG_EN:LANG_ZH;save_config(c);}else if(action==12)break;
    }
    return 0;
}

#include "modules/command-sets/command_sets_ui.inc"

static int template_has_placeholder(const char *cmd, int n) {
    char p[4] = {'{', (char)('0' + n), '}', '\0'};
    return strstr(cmd, p) != NULL;
}

static int shell_syntax_ok(const char *cmd) {
    pid_t p = fork();
    if (p == 0) {
        execl("/bin/sh", "sh", "-n", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    if (p < 0) return 0;
    int st = 0;
    while (waitpid(p, &st, 0) == -1 && errno == EINTR) {}
    return WIFEXITED(st) && WEXITSTATUS(st) == 0;
}

static const Task *task_by_id(const char *id) {
    for (size_t i = 0; i < TASK_COUNT; ++i) if (strcmp(TASKS[i].id, id) == 0) return &TASKS[i];
    return NULL;
}
static int parse_profile_arg(const char *s,LinuxProfile *out){
    if(!strcmp(s,"generic")){*out=PROFILE_GENERIC;return 0;}
    if(!strcmp(s,"centos")){*out=PROFILE_CENTOS;return 0;}
    return -1;
}
static int parse_config_profile_arg(const char*s,LinuxProfile*out){return parse_profile_arg(s,out);}
static int print_catalogue_info(LinuxProfile p){
    if(rebuild_effective_catalogue(p))return 1;
    size_t counts[CAT_COUNT]={0},centos=0;
    for(size_t i=0;i<TASK_COUNT;i++){counts[TASKS[i].category]++;if(TASKS[i].source_id&&!strcmp(TASKS[i].source_id,"centos"))centos++;}
    printf("profile=%s actions=%zu centos_source=%zu overrides=%zu packages=%zu services=%zu firewall=%zu selinux=%zu\n",
           linux_profile_key(p),TASK_COUNT,centos,g_overlay_override_count,counts[CAT_PACKAGE],counts[CAT_SERVICE],counts[CAT_FIREWALL],counts[CAT_SELINUX]);
    return 0;
}
static int print_visible_categories(LinuxProfile p){
    if(rebuild_effective_catalogue(p))return 1;
    Category cats[CAT_COUNT];size_t n=collect_visible_categories(cats,ARRAY_LEN(cats));
    printf("profile=%s categories=",linux_profile_key(p));for(size_t i=0;i<n;i++){if(i)putchar(',');fputs(category_name(cats[i],LANG_EN),stdout);}putchar('\n');return 0;
}
static int print_task_info(LinuxProfile p,const char *id){
    if(rebuild_effective_catalogue(p))return 1;
    const Task*t=task_by_id(id);if(!t)return 2;
    printf("id=%s source=%s category=%s risk=%d command=%s\n",t->id,t->source_id?t->source_id:"unknown",category_name(t->category,LANG_EN),(int)t->risk,t->command);return 0;
}

typedef struct {
    size_t actions;
    size_t centos_source;
    size_t counts[CAT_COUNT];
    size_t syntax_checked;
} SelfStats;

static size_t validate_current_catalogue(const char *label,SelfStats *stats){
    static const char *banned[]={"rm -rf /","rm -fr /","mkfs.","dd if=/dev/zero of=/dev/","dd if=/dev/random of=/dev/","nft flush ruleset","iptables -F"};
    size_t errors=0;memset(stats,0,sizeof(*stats));stats->actions=TASK_COUNT;
    for(size_t i=0;i<TASK_COUNT;i++){
        const Task*t=&TASKS[i];
        if(!t->id||!*t->id||!t->title_zh||!t->title_en||!t->command||!*t->command||!t->source_id||!*t->source_id){fprintf(stderr,"SELFTEST %s invalid task at index %zu\n",label,i);errors++;}
        if(t->category<=CAT_ALL||t->category>=CAT_COUNT||t->risk<RISK_NORMAL||t->risk>RISK_PRIVILEGED){fprintf(stderr,"SELFTEST %s invalid enum values for %s\n",label,t->id?t->id:"(null)");errors++;}
        if(t->source_id&&strcmp(t->source_id,"linux-core")&&strcmp(t->source_id,"centos")){fprintf(stderr,"SELFTEST %s invalid source: %s\n",label,t->source_id);errors++;}
        if(t->source_id&&!strcmp(t->source_id,"centos"))stats->centos_source++;
        if(t->category>CAT_ALL&&t->category<CAT_COUNT)stats->counts[t->category]++;
        for(size_t j=i+1;j<TASK_COUNT;j++)if(t->id&&TASKS[j].id&&!strcmp(t->id,TASKS[j].id)){fprintf(stderr,"SELFTEST %s duplicate id: %s\n",label,t->id);errors++;}
        for(int arg=1;arg<=WB_MAX_ARGS;arg++){
            int defined=t->args[arg-1].kind!=ARG_NONE;
            if(template_has_placeholder(t->command,arg)!=defined){fprintf(stderr,"SELFTEST %s placeholder/arg%d mismatch: %s\n",label,arg,t->id);errors++;}
        }
        for(size_t b=0;b<ARRAY_LEN(banned);b++)if(strstr(t->command,banned[b])){fprintf(stderr,"SELFTEST %s banned pattern in %s: %s\n",label,t->id,banned[b]);errors++;}
        char sample[WB_MAX_ARGS][WB_INPUT*4]={{0}};
        for(size_t ai=0;ai<WB_MAX_ARGS;ai++){
            const char*v="x";
            if(t->args[ai].kind==ARG_PATH||t->args[ai].kind==ARG_DESTRUCTIVE_PATH)v="/tmp/wb-selftest";
            else if(t->args[ai].kind==ARG_UINT||t->args[ai].kind==ARG_PID)v="1";
            else if(t->args[ai].kind==ARG_PORT)v="80";
            if(t->args[ai].kind!=ARG_NONE&&shell_quote(v,sample[ai],sizeof(sample[ai]))){fprintf(stderr,"SELFTEST %s sample quote failed: %s arg%zu\n",label,t->id,ai+1);errors++;}
        }
        char rendered[MAX_COMMAND]={0};
        if(!substitute_template(t->command,sample,rendered,sizeof(rendered))||!shell_syntax_ok(rendered)){fprintf(stderr,"SELFTEST %s shell syntax failed: %s\n  %s\n",label,t->id,rendered);errors++;}
        else stats->syntax_checked++;
    }
    return errors;
}

static int selftest_pseudo_fs(const char *fs){
    static const char *pseudo[]={"proc","sysfs","devtmpfs","devpts","tmpfs","ramfs","cgroup","cgroup2","pstore","securityfs","debugfs","tracefs","configfs","hugetlbfs","mqueue","autofs","fusectl","binfmt_misc","rpc_pipefs","nsfs","efivarfs"};
    for(size_t i=0;i<ARRAY_LEN(pseudo);i++)if(fs&&!strcmp(fs,pseudo[i]))return 1;
    return 0;
}

static int self_test_catalogue(void){
    size_t errors=0;SelfStats generic,centos;
    if(rebuild_effective_catalogue(PROFILE_GENERIC))return 1;
    errors+=validate_current_catalogue("generic",&generic);
    if(generic.actions!=linux_core_module()->task_count||generic.centos_source!=0||generic.counts[CAT_PACKAGE]||generic.counts[CAT_SERVICE]||generic.counts[CAT_FIREWALL]||generic.counts[CAT_SELINUX]){fprintf(stderr,"SELFTEST generic profile isolation failed\n");errors++;}
    const Task*files_list=task_by_id("files.list");const Task*files_hidden=task_by_id("files.list_hidden");int search_checks=0;
    if(files_list&&task_matches(files_list,LANG_ZH,CAT_FILES,"当前目录文件"))search_checks++;
    if(files_hidden&&task_matches(files_hidden,LANG_EN,CAT_FILES,"dotfiles"))search_checks++;
    if(files_list&&task_matches(files_list,LANG_EN,CAT_FILES,"-lah"))search_checks++;
    if(files_list&&task_matches(files_list,LANG_EN,CAT_FILES,"files.list"))search_checks++;
    if(files_list&&!task_matches(files_list,LANG_EN,CAT_NETWORK,"files.list"))search_checks++;
    if(search_checks!=5){fprintf(stderr,"SELFTEST search semantics failed: %d/5\n",search_checks);errors++;}

    if(rebuild_effective_catalogue(PROFILE_CENTOS))return 1;
    size_t overrides=g_overlay_override_count;
    errors+=validate_current_catalogue("centos",&centos);
    const Task*pkg=task_by_id("centos.packages.search");const Task*host=task_by_id("system.hostname");
    if(!pkg||strcmp(pkg->source_id,"centos")||!host||strcmp(host->source_id,"centos")||overrides<1){fprintf(stderr,"SELFTEST centos overlay semantics failed\n");errors++;}

    int path_guard_ok=!destructive_path_allowed("/")&&!destructive_path_allowed("//")&&!destructive_path_allowed(".")&&!destructive_path_allowed("..")&&destructive_path_allowed("/tmp/wb-selftest");
    if(!path_guard_ok){fprintf(stderr,"SELFTEST destructive path guard failed\n");errors++;}
    mode_t fm_mode=0;
    int files_guard_ok=fm_parse_mode("0640",&fm_mode)==0&&fm_mode==0640&&fm_parse_mode("0888",&fm_mode)!=0&&
        !fm_destructive_path_allowed("/")&&!fm_destructive_path_allowed(".")&&!fm_destructive_path_allowed("..")&&fm_destructive_path_allowed("/tmp/wb-files-selftest");
    if(!files_guard_ok){fprintf(stderr,"SELFTEST files manager guard failed\n");errors++;}
    int locations_guard_ok=0;FmLocations self_locations={0};char location_err[256]={0};
    if(fm_load_locations("/proc/self/mountinfo",0,&self_locations,location_err,sizeof(location_err))==0){
        int saw_root=0,saw_pseudo=0;
        for(size_t i=0;i<self_locations.count;i++){if(!strcmp(self_locations.items[i].mount_point,"/"))saw_root=1;if(strcmp(self_locations.items[i].mount_point,"/")&&selftest_pseudo_fs(self_locations.items[i].fs_type))saw_pseudo=1;}
        locations_guard_ok=saw_root&&!saw_pseudo;fm_free_locations(&self_locations);
    }
    if(!locations_guard_ok){fprintf(stderr,"SELFTEST locations guard failed: %s\n",location_err[0]?location_err:"root/pseudo invariant");errors++;}
    UiEvent hit_event={UI_EVENT_MOUSE_PRESS,0,4,5,0};
    char edit_guard[8]="ab";
    ui_hits_reset();ui_hit_add(77,2,5,8,5);
    ui_utf8_backspace(edit_guard);
    int dual_input_guard_ok=ui_hit_test(&hit_event)==77&&ui_append_byte(edit_guard,sizeof(edit_guard),'x')&&strcmp(edit_guard,"ax")==0;
    if(!dual_input_guard_ok){fprintf(stderr,"SELFTEST dual input guard failed\n");errors++;}
    WbTerminalSession term_guard={0};
    int terminal_guard_ok=0;
    if(wb_terminal_init(&term_guard,2)==0){
        static const unsigned char sample[]="a\nb\nc\n";
        wb_terminal_feed(&term_guard,sample,sizeof(sample)-1);
        wb_terminal_clear_screen(&term_guard);
        terminal_guard_ok=wb_terminal_line_count(&term_guard)==2&&
            wb_terminal_line_at(&term_guard,0)&&!strcmp(wb_terminal_line_at(&term_guard,0),"b")&&
            wb_terminal_line_at(&term_guard,1)&&!strcmp(wb_terminal_line_at(&term_guard,1),"c")&&
            wb_terminal_screen_floor(&term_guard)==2;
        wb_terminal_destroy(&term_guard);
    }
    if(!terminal_guard_ok){fprintf(stderr,"SELFTEST terminal guard failed\n");errors++;}
    CommandSet command_guard={0};
    const Module *core=linux_core_module();
    int command_sets_guard_ok=wb_command_set_init_system_linux(&command_guard,core->tasks,core->task_count)==0&&
        command_guard.source==COMMAND_SET_SYSTEM&&command_guard.read_only==1&&
        wb_command_set_task_count(&command_guard)==core->task_count&&!wb_command_set_can_edit(&command_guard)&&
        wb_command_set_rename(&command_guard,"Changed","Changed")!=0&&wb_command_set_mark_deleted(&command_guard)!=0;
    wb_command_set_destroy(&command_guard);
    if(!command_sets_guard_ok){fprintf(stderr,"SELFTEST command sets guard failed\n");errors++;}
    if(errors){fprintf(stderr,"SELFTEST FAIL profiles=2 errors=%zu\n",errors);return 1;}
    printf("SELFTEST PASS profiles=2 generic_actions=%zu centos_actions=%zu centos_source=%zu overrides=%zu max_args=%d generic_system=%zu generic_files=%zu generic_text=%zu generic_process=%zu generic_network=%zu generic_storage=%zu generic_permission=%zu generic_archive=%zu generic_user=%zu centos_packages=%zu centos_services=%zu centos_firewall=%zu centos_selinux=%zu generic_syntax=%zu centos_syntax=%zu destructive_path_guard=%s files_guard=%s locations_guard=%s dual_input_guard=%s terminal_guard=%s command_sets_guard=%s search_checked=%d\n",
           generic.actions,centos.actions,centos.centos_source,overrides,WB_MAX_ARGS,
           generic.counts[CAT_SYSTEM],generic.counts[CAT_FILES],generic.counts[CAT_TEXT],generic.counts[CAT_PROCESS],generic.counts[CAT_NETWORK],generic.counts[CAT_STORAGE],generic.counts[CAT_PERMISSION],generic.counts[CAT_ARCHIVE],generic.counts[CAT_USER],
           centos.counts[CAT_PACKAGE],centos.counts[CAT_SERVICE],centos.counts[CAT_FIREWALL],centos.counts[CAT_SELINUX],generic.syntax_checked,centos.syntax_checked,path_guard_ok?"PASS":"FAIL",files_guard_ok?"PASS":"FAIL",locations_guard_ok?"PASS":"FAIL",dual_input_guard_ok?"PASS":"FAIL",terminal_guard_ok?"PASS":"FAIL",command_sets_guard_ok?"PASS":"FAIL",search_checks);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--self-test") == 0) return self_test_catalogue();
    if (argc == 2 && strcmp(argv[1], "--profile-info") == 0) {
        AppConfig c;DetectedLinux d;load_config(&c);detect_linux(&d);LinuxProfile e=effective_linux_profile(&c,&d);
        if(rebuild_effective_catalogue(e))return 1;
        printf("configured=%s detected=%s detected_id=%s detected_version=%s effective=%s actions=%zu\n",
               linux_profile_key(c.linux_profile),linux_profile_key(d.profile),d.id,d.version[0]?d.version:"unknown",
               linux_profile_key(e),TASK_COUNT);
        return 0;
    }
    if(argc==3&&strcmp(argv[1],"--set-profile")==0){AppConfig c;LinuxProfile p;if(parse_config_profile_arg(argv[2],&p))return 2;load_config(&c);c.linux_profile=p;return save_config(&c)?1:0;}
    if(argc==3&&strcmp(argv[1],"--catalogue-info")==0){LinuxProfile p;if(parse_profile_arg(argv[2],&p))return 2;return print_catalogue_info(p);}
    if(argc==3&&strcmp(argv[1],"--visible-categories")==0){LinuxProfile p;if(parse_profile_arg(argv[2],&p))return 2;return print_visible_categories(p);}
    if(argc==4&&strcmp(argv[1],"--task-info")==0){LinuxProfile p;if(parse_profile_arg(argv[2],&p))return 2;return print_task_info(p,argv[3]);}
    signal(SIGINT,fatal_signal);signal(SIGTERM,fatal_signal);signal(SIGHUP,fatal_signal);atexit(restore_terminal);
    AppConfig c;load_config(&c);if(rebuild_from_config(&c))return 1;
    char command_root[4096];if(wb_command_sets_default_root(command_root,sizeof(command_root))||wb_command_sets_init(&g_command_sets,TASKS,TASK_COUNT,command_root))return 1;
    g_command_sets_initialized=1;if(wb_command_sets_load_user(&g_command_sets))return 1;
    if(enable_raw_mode())return 1;
    size_t sel=0;
    for(;;){
        const size_t total=5;if(sel>=total)sel=0;draw_desktop(sel,c.language);int k=read_key();if(k==-1)break;
        if(k=='q'||k=='Q'||k==27)break;
        if(k==KEY_UP||k=='k'||k=='K')sel=sel?sel-1:total-1;
        else if(k==KEY_DOWN||k=='j'||k=='J')sel=(sel+1)%total;
        else if(k=='s'||k=='S')settings_menu(&c);
        else if(k=='l'||k=='L'){c.language=c.language==LANG_ZH?LANG_EN:LANG_ZH;save_config(&c);}
        else if(k==KEY_MOUSE&&!mouse_release&&mouse_button==0){int ro=mouse_y-6;if(ro>=0&&(size_t)ro<total){sel=(size_t)ro;if(sel==0)files_manager_menu(&c);else if(sel==1)terminal_menu(&c);else if(sel==2)command_sets_menu(&c,COMMAND_MODE_EXECUTE,NULL,0);else if(sel==3)settings_menu(&c);else break;}}
        else if(k=='\r'||k=='\n'){if(sel==0)files_manager_menu(&c);else if(sel==1)terminal_menu(&c);else if(sel==2)command_sets_menu(&c,COMMAND_MODE_EXECUTE,NULL,0);else if(sel==3)settings_menu(&c);else break;}
    }
    if(g_terminal_session_initialized){wb_terminal_destroy(&g_terminal_session);g_terminal_session_initialized=0;}
    if(g_command_sets_initialized){wb_command_sets_free(&g_command_sets);g_command_sets_initialized=0;}
    clear_screen();printf("%s\n",tr(c.language,"Workbench 已退出。","Workbench exited."));return 0;
}
