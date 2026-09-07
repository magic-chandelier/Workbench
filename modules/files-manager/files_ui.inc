/* Workbench Files TUI. Included by main.c after core prompt/settings helpers. */

static size_t fm_locations_visible_start = 0, fm_locations_visible_end = 0;
static const int fm_locations_row_start_y = 7;
static size_t fm_browser_visible_start = 0, fm_browser_visible_end = 0;
static const int fm_browser_row_start_y = 8;

static void fm_sanitize_text(const char *src, char *dst, size_t n) {
    if (!dst || n == 0) return;
    size_t u = 0;
    if (!src) src = "";
    for (const unsigned char *p = (const unsigned char *)src; *p && u + 1 < n; ++p) {
        unsigned char c = *p;
        dst[u++] = (c < 0x20 || c == 0x7f) ? '?' : (char)c;
    }
    dst[u] = '\0';
}

static const char *fm_type_name(FmEntryType type, Language l) {
    switch (type) {
        case FM_ENTRY_DIRECTORY: return tr(l, "目录", "Directory");
        case FM_ENTRY_REGULAR: return tr(l, "文件", "File");
        case FM_ENTRY_SYMLINK: return tr(l, "符号链接", "Symlink");
        default: return tr(l, "其他", "Other");
    }
}

static void fm_mode_text(mode_t mode, char out[8]) {
    snprintf(out, 8, "%04o", (unsigned)(mode & 07777));
}

static void fm_size_text(off_t size, int is_dir, char *out, size_t n) {
    if (is_dir) { snprintf(out, n, "-"); return; }
    double value = (double)(size < 0 ? 0 : size);
    const char *unit = "B";
    if (value >= 1024.0) { value /= 1024.0; unit = "K"; }
    if (value >= 1024.0) { value /= 1024.0; unit = "M"; }
    if (value >= 1024.0) { value /= 1024.0; unit = "G"; }
    if (value >= 1024.0) { value /= 1024.0; unit = "T"; }
    if (unit[0] == 'B') snprintf(out, n, "%lld B", (long long)size);
    else snprintf(out, n, value >= 10.0 ? "%.0f%s" : "%.1f%s", value, unit);
}

static void fm_show_error(AppConfig *c, const char *zh_action, const char *en_action, const char *err) {
    char zh[512], en[512], safe[320];
    fm_sanitize_text(err && *err ? err : strerror(errno), safe, sizeof(safe));
    snprintf(zh, sizeof(zh), "%s失败：%s", zh_action, safe);
    snprintf(en, sizeof(en), "%s failed: %s", en_action, safe);
    show_message(c->language, zh, en);
}

static int fm_valid_name(const char *name) {
    if (!name || !*name || !strcmp(name, ".") || !strcmp(name, "..") || strchr(name, '/')) return 0;
    for (const unsigned char *p = (const unsigned char *)name; *p; ++p)
        if (*p < 0x20 || *p == 0x7f) return 0;
    return 1;
}

static int fm_prompt_name(AppConfig *c, const char *title_zh, const char *title_en,
                          const char *prompt_zh, const char *prompt_en, char *out, size_t n) {
    if (!prompt_text(c->language, tr(c->language, title_zh, title_en), prompt_zh, prompt_en,
                     NULL, ARG_TEXT, out, n)) return 0;
    if (!fm_valid_name(out)) {
        show_message(c->language,
                     "名称不能为空，不能是 . 或 ..，不能包含 / 或控制字符。",
                     "Name cannot be empty, . or .., and cannot contain / or control characters.");
        return 0;
    }
    return 1;
}

static int fm_resolve_destination(const char *current, const char *input, char *out, size_t n) {
    if (!input || !*input) { errno = EINVAL; return -1; }
    if (input[0] == '/') {
        if (strlen(input) + 1 > n) { errno = ENAMETOOLONG; return -1; }
        strcpy(out, input);
        return 0;
    }
    return fm_join_path(current, input, out, n);
}

static void fm_format_time(time_t when, char *out, size_t n) {
    struct tm tmv;
    if (localtime_r(&when, &tmv) && strftime(out, n, "%Y-%m-%d %H:%M:%S", &tmv)) return;
    snprintf(out, n, "-");
}

static FmEntryType fm_type_from_mode(mode_t mode) {
    if (S_ISDIR(mode)) return FM_ENTRY_DIRECTORY;
    if (S_ISREG(mode)) return FM_ENTRY_REGULAR;
    if (S_ISLNK(mode)) return FM_ENTRY_SYMLINK;
    return FM_ENTRY_OTHER;
}

static void fm_properties(AppConfig *c, const char *path) {
    struct stat st;if(lstat(path,&st)!=0){fm_show_error(c,"读取属性","Read properties",strerror(errno));return;}
    char safe_path[PATH_MAX],mode[8],size[64],when[64],owner[96],group[96];fm_sanitize_text(path,safe_path,sizeof(safe_path));fm_mode_text(st.st_mode,mode);fm_size_text(st.st_size,S_ISDIR(st.st_mode),size,sizeof(size));fm_format_time(st.st_mtime,when,sizeof(when));struct passwd*pw=getpwuid(st.st_uid);struct group*gr=getgrgid(st.st_gid);snprintf(owner,sizeof(owner),"%s (%lu)",pw?pw->pw_name:"-",(unsigned long)st.st_uid);snprintf(group,sizeof(group),"%s (%lu)",gr?gr->gr_name:"-",(unsigned long)st.st_gid);
    for(;;){Language l=c->language;int w=terminal_width();if(w>110)w=110;clear_screen();ui_hits_reset();printf("\033[?25lWorkbench %s | Workbench Files > %s\n",WB_VERSION,tr(l,"属性","Properties"));print_rule(w);int x=1;ui_print_button(1,3,&x,tr(l,"返回","Back"));ui_print_button(2,3,&x,tr(l,"语言","Language"));putchar('\n');printf("%s: ",tr(l,"路径","Path"));print_truncated(safe_path,w-8);putchar('\n');printf("%s: %s\n",tr(l,"类型","Type"),fm_type_name(fm_type_from_mode(st.st_mode),l));printf("%s: %s\n",tr(l,"大小","Size"),size);printf("%s: %s\n",tr(l,"权限","Mode"),mode);printf("%s: %s\n",tr(l,"所有者","Owner"),owner);printf("%s: %s\n",tr(l,"用户组","Group"),group);printf("%s: %s\n",tr(l,"修改时间","Modified"),when);if(S_ISLNK(st.st_mode)){char target[PATH_MAX],safe_target[PATH_MAX];ssize_t nr=readlink(path,target,sizeof(target)-1);if(nr>=0){target[nr]='\0';fm_sanitize_text(target,safe_target,sizeof(safe_target));printf("%s: ",tr(l,"链接到","Links to"));print_truncated(safe_target,w-10);putchar('\n');}}fflush(stdout);
        UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)return;if(ev.kind==UI_EVENT_KEY){int k=ev.key;if(k==27||k=='q'||k=='Q'||k=='\r'||k=='\n')return;if(k=='l'||k=='L'){c->language=c->language==LANG_ZH?LANG_EN:LANG_ZH;save_config(c);}}else if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0){int hit=ui_hit_test(&ev);if(hit==1)return;if(hit==2){c->language=c->language==LANG_ZH?LANG_EN:LANG_ZH;save_config(c);}}
    }
}

static int fm_file_is_binary(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    unsigned char buf[4096]; ssize_t nr;
    do { nr = read(fd, buf, sizeof(buf)); } while (nr < 0 && errno == EINTR);
    int saved = errno; close(fd); errno = saved;
    if (nr < 0) return -1;
    for (ssize_t i = 0; i < nr; ++i) if (buf[i] == 0) return 1;
    return 0;
}

static void fm_print_view_line(const char *line, int cols) {
    int used = 0;
    const unsigned char *p = (const unsigned char *)line;
    while (*p && *p != '\n' && *p != '\r' && used < cols) {
        if (*p == '\t') {
            int spaces = 4 - (used % 4);
            while (spaces-- > 0 && used < cols) { putchar(' '); used++; }
            p++;
            continue;
        }
        if (*p < 0x20 || *p == 0x7f) { putchar('?'); used++; p++; continue; }
        unsigned cp = 0;
        size_t bytes = utf8_decode_one(p, &cp);
        int width = unicode_cell_width(cp);
        if (width < 1) width = 1;
        if (used + width > cols) break;
        fwrite(p, 1, bytes, stdout);
        p += bytes;
        used += width;
    }
}

static void fm_view_file(AppConfig *c, const char *path) {
    struct stat st;if(stat(path,&st)!=0||!S_ISREG(st.st_mode)){show_message(c->language,"只能查看普通文件。","Only regular files can be viewed.");return;}int binary=fm_file_is_binary(path);if(binary<0){fm_show_error(c,"打开文件","Open file",strerror(errno));return;}if(binary){show_message(c->language,"检测到二进制内容，内置查看器不会直接输出该文件。","Binary content detected; the built-in viewer will not print it to the terminal.");return;}size_t page=0;
    for(;;){FILE*f=fopen(path,"r");if(!f){fm_show_error(c,"打开文件","Open file",strerror(errno));return;}int w=terminal_width(),h=terminal_height();if(w>1)w--;if(w>118)w=118;if(h<12)h=12;int per=h-7;if(per<4)per=4;size_t skip=page*(size_t)per,line_no=0;char line[4096];while(line_no<skip&&fgets(line,sizeof(line),f))line_no++;clear_screen();ui_hits_reset();printf("\033[?25lWorkbench %s | Workbench Files > %s\n",WB_VERSION,tr(c->language,"查看文件","View file"));print_rule(w);printf("%s: ",tr(c->language,"文件","File"));char safe[PATH_MAX];fm_sanitize_text(path,safe,sizeof(safe));print_truncated(safe,w-8);putchar('\n');int x=1;ui_print_button(1,4,&x,tr(c->language,"上一页","Previous"));ui_print_button(2,4,&x,tr(c->language,"下一页","Next"));ui_print_button(3,4,&x,tr(c->language,"返回","Back"));putchar('\n');print_rule(w);int shown=0;while(shown<per&&fgets(line,sizeof(line),f)){printf("%6zu  ",line_no+1);fm_print_view_line(line,w-9);putchar('\n');line_no++;shown++;}int has_more=fgets(line,sizeof(line),f)!=NULL;fclose(f);printf("\n%s: %zu",tr(c->language,"页","Page"),page+1);if(!has_more)printf("  %s",tr(c->language,"[末页]","[end]"));fflush(stdout);
        UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)return;int action=0;if(ev.kind==UI_EVENT_KEY){int k=ev.key;if(k==27||k=='q'||k=='Q'||k==KEY_LEFT)return;if(k==KEY_UP||k=='k'||k=='K')action=1;else if(k==KEY_DOWN||k=='j'||k=='J'||k==' '||k=='\r'||k=='\n')action=2;}else if(ev.kind==UI_EVENT_WHEEL_UP)action=1;else if(ev.kind==UI_EVENT_WHEEL_DOWN)action=2;else if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0)action=ui_hit_test(&ev);if(action==3)return;if(action==1){if(page)page--;}else if(action==2&&has_more)page++;
    }
}

static const char *fm_location_kind_name(FmLocationKind kind, Language l) {
    if (kind == FM_LOCATION_NETWORK) return tr(l, "网络存储", "Network");
    if (kind == FM_LOCATION_SYSTEM) return tr(l, "系统", "System");
    return tr(l, "本地存储", "Local");
}

static void fm_capacity_text(const FmLocation *loc, char *out, size_t n) {
    if (!loc || !loc->capacity_known || loc->total_bytes == 0) { snprintf(out, n, "-"); return; }
    uint64_t used = loc->total_bytes > loc->available_bytes ? loc->total_bytes - loc->available_bytes : 0;
    unsigned pct = (unsigned)((used * 100u) / loc->total_bytes);
    double total = (double)loc->total_bytes;
    const char *unit = "B";
    if (total >= 1024.0) { total /= 1024.0; unit = "K"; }
    if (total >= 1024.0) { total /= 1024.0; unit = "M"; }
    if (total >= 1024.0) { total /= 1024.0; unit = "G"; }
    if (total >= 1024.0) { total /= 1024.0; unit = "T"; }
    snprintf(out, n, total >= 10.0 ? "%.0f%s %u%%" : "%.1f%s %u%%", total, unit, pct);
}

static int fm_locations_selected_path(const FmLocations *locations, size_t sel, char *out, size_t n) {
    if (sel == 0) {
        const char *home = getenv("HOME");
        if (!home || !*home) { errno = ENOENT; return -1; }
        char resolved[PATH_MAX];
        if (!realpath(home, resolved)) return -1;
        if (strlen(resolved) + 1 > n) { errno = ENAMETOOLONG; return -1; }
        strcpy(out, resolved);
        return 0;
    }
    size_t idx = sel - 1;
    if (!locations || idx >= locations->count) { errno = EINVAL; return -1; }
    if (strlen(locations->items[idx].mount_point) + 1 > n) { errno = ENAMETOOLONG; return -1; }
    strcpy(out, locations->items[idx].mount_point);
    return 0;
}

static void fm_draw_locations(AppConfig *c, const FmLocations *locations, int show_system, size_t sel) {
    int w=terminal_width(),h=terminal_height();if(w>1)w--;if(w>118)w=118;if(h<18)h=18;size_t total=locations->count+1;clear_screen();ui_hits_reset();printf("\033[?25lWorkbench %s | Workbench Files > %s\n",WB_VERSION,tr(c->language,"位置","Locations"));print_rule(w);int x=1;ui_print_button(10,3,&x,show_system?tr(c->language,"隐藏系统挂载","Hide system mounts"):tr(c->language,"显示系统挂载","Show system mounts"));ui_print_button(11,3,&x,tr(c->language,"语言","Language"));ui_print_button(12,3,&x,tr(c->language,"桌面","Desktop"));putchar('\n');printf("%s: %s\n",tr(c->language,"系统挂载","System mounts"),show_system?tr(c->language,"显示","shown"):tr(c->language,"隐藏","hidden"));print_rule(w);printf("   ");print_cell(tr(c->language,"位置","Location"),16);print_cell(tr(c->language,"挂载点","Mount point"),36);print_cell(tr(c->language,"来源","Source"),24);print_cell(tr(c->language,"文件系统","FS"),10);print_cell(tr(c->language,"容量","Capacity"),12);putchar('\n');int visible=h-10;if(visible<5)visible=5;size_t start=0;if(total>(size_t)visible&&sel>=(size_t)visible)start=sel-(size_t)visible+1;size_t end=start+(size_t)visible;if(end>total)end=total;fm_locations_visible_start=start;fm_locations_visible_end=end;
    for(size_t row=start;row<end;++row){char mount_safe[PATH_MAX],source_safe[PATH_MAX],cap[32];const char*kind,*mount,*source,*fs,*access="";if(row==0){const char*home=getenv("HOME");kind=tr(c->language,"主目录","Home");mount=home&&*home?home:"-";source="-";fs="-";snprintf(cap,sizeof(cap),"-");}else{const FmLocation*loc=&locations->items[row-1];kind=fm_location_kind_name(loc->kind,c->language);mount=loc->mount_point;source=loc->source;fs=loc->fs_type;fm_capacity_text(loc,cap,sizeof(cap));access=loc->read_only?" RO":" RW";}fm_sanitize_text(mount,mount_safe,sizeof(mount_safe));fm_sanitize_text(source,source_safe,sizeof(source_safe));int y=fm_locations_row_start_y+(int)(row-start);ui_hit_add(100+(int)row,1,y,w,y);if(row==sel)printf("\033[7m > ");else printf("   ");print_cell(kind,16);print_cell(mount_safe,36);print_cell(source_safe,24);print_cell(fs,10);print_cell(cap,12);fputs(access,stdout);if(row==sel)printf("\033[0m");putchar('\n');}
    putchar('\n');print_rule(w);printf("%s: %zu   %s: %zu/%zu\n",tr(c->language,"挂载点","Mounts"),locations->count,tr(c->language,"选择","Selected"),sel+1,total);if(g_shortcut_hints_enabled)printf("%s\n",tr(c->language,"快捷键：Enter 打开 | h 系统挂载 | l 语言 | q 桌面","Shortcuts: Enter Open | h System mounts | l Language | q Desktop"));fflush(stdout);
}

static int fm_locations_menu(AppConfig *c, char *out_path, size_t outn) {
    int show_system=0;size_t sel=0;
    for(;;){FmLocations locations={0};char err[256];const char*mountinfo=getenv("WB_MOUNTINFO");if(fm_load_locations(mountinfo,show_system,&locations,err,sizeof(err))!=0){fm_show_error(c,"读取挂载点","Read mounts",err);return 0;}size_t total=locations.count+1;if(sel>=total)sel=total?total-1:0;fm_draw_locations(c,&locations,show_system,sel);UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF){fm_free_locations(&locations);return 0;}int action=0;
        if(ev.kind==UI_EVENT_KEY){int k=ev.key;if(k==27||k=='q'||k=='Q'){fm_free_locations(&locations);return 0;}if(k==KEY_UP||k=='k'||k=='K'){if(total)sel=sel?sel-1:total-1;}else if(k==KEY_DOWN||k=='j'||k=='J'){if(total)sel=(sel+1)%total;}else if(k=='h'||k=='H')action=10;else if(k=='l'||k=='L')action=11;else if(k=='\r'||k=='\n')action=20;}
        else if(ev.kind==UI_EVENT_WHEEL_UP){if(total)sel=sel?sel-1:total-1;}else if(ev.kind==UI_EVENT_WHEEL_DOWN){if(total)sel=(sel+1)%total;}
        else if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0){int hit=ui_hit_test(&ev);if(hit>=100){sel=(size_t)(hit-100);action=20;}else action=hit;}
        if(action==10){show_system=!show_system;sel=0;fm_free_locations(&locations);continue;}if(action==11){c->language=c->language==LANG_ZH?LANG_EN:LANG_ZH;save_config(c);fm_free_locations(&locations);continue;}if(action==12){fm_free_locations(&locations);return 0;}if(action==20){int rc=fm_locations_selected_path(&locations,sel,out_path,outn);fm_free_locations(&locations);if(rc==0)return 1;fm_show_error(c,"打开位置","Open location",strerror(errno));continue;}fm_free_locations(&locations);
    }
}

static int fm_selected_path(const char *current, const FmDirectory *dir, int has_parent, size_t sel,
                            char *out, size_t n, const FmEntry **entry) {
    if (has_parent && sel == 0) { if (entry) *entry = NULL; return fm_parent_path(current, out, n); }
    size_t idx = sel - (has_parent ? 1u : 0u);
    if (idx >= dir->count) { errno = EINVAL; return -1; }
    if (entry) *entry = &dir->entries[idx];
    return fm_join_path(current, dir->entries[idx].name, out, n);
}

static void fm_draw_browser(AppConfig *c, const char *current, const FmDirectory *dir, int show_hidden, size_t sel) {
    int w=terminal_width(),h=terminal_height();if(w>1)w--;if(w>118)w=118;if(h<18)h=18;int has_parent=strcmp(current,"/")!=0;size_t total=dir->count+(size_t)has_parent;clear_screen();ui_hits_reset();printf("\033[?25lWorkbench %s | Workbench Files\n",WB_VERSION);print_rule(w);printf("%s: ",tr(c->language,"当前路径","Current path"));char safe[PATH_MAX];fm_sanitize_text(current,safe,sizeof(safe));print_truncated(safe,w-14);putchar('\n');int x=1;ui_print_button(10,4,&x,tr(c->language,"位置","Locations"));ui_print_button(11,4,&x,tr(c->language,"主目录","Home"));if(g_shortcut_hints_enabled)printf(" %s",tr(c->language,"键盘：↑/↓ Enter ←/Backspace；鼠标：单击/双击/右键","Keyboard: Up/Down Enter Left/Backspace; Mouse: click/double-click/right-click"));putchar('\n');x=1;ui_print_button(20,5,&x,tr(c->language,"新建文件","New file"));ui_print_button(21,5,&x,tr(c->language,"新建目录","New dir"));ui_print_button(22,5,&x,tr(c->language,"隐藏文件","Hidden"));ui_print_button(23,5,&x,tr(c->language,"语言","Language"));ui_print_button(24,5,&x,tr(c->language,"桌面","Desktop"));putchar('\n');printf("%s: %s\n",tr(c->language,"隐藏文件","Hidden files"),show_hidden?tr(c->language,"显示","shown"):tr(c->language,"隐藏","hidden"));print_rule(w);int visible=h-(g_shortcut_hints_enabled?12:10);if(visible<5)visible=5;size_t start=0;if(total>(size_t)visible&&sel>=(size_t)visible)start=sel-(size_t)visible+1;size_t end=start+(size_t)visible;if(end>total)end=total;fm_browser_visible_start=start;fm_browser_visible_end=end;if(!total)printf("  %s\n",tr(c->language,"目录为空。","Directory is empty."));
    for(size_t row=start;row<end;++row){int parent_row=has_parent&&row==0;const FmEntry*e=parent_row?NULL:&dir->entries[row-(size_t)has_parent];char name[PATH_MAX],mode[8],size[32];fm_sanitize_text(parent_row?"..":e->name,name,sizeof(name));if(parent_row){strcpy(mode,"----");strcpy(size,"-");}else{fm_mode_text(e->mode,mode);fm_size_text(e->size,e->type==FM_ENTRY_DIRECTORY,size,sizeof(size));}if(row==sel)printf("\033[7m > ");else printf("   ");print_cell(name,38);print_cell(parent_row?tr(c->language,"目录","Directory"):fm_type_name(e->type,c->language),12);print_cell(size,10);print_cell(mode,6);if(row==sel)printf("\033[0m");putchar('\n');}
    putchar('\n');print_rule(w);printf("%s: %zu",tr(c->language,"项目","Items"),dir->count);if(total)printf("   %s: %zu/%zu",tr(c->language,"选择","Selected"),sel+1,total);putchar('\n');if(g_shortcut_hints_enabled){printf("%s\n",tr(c->language,"快捷键：n 新建文件 | N 新建目录 | v 查看 | p 属性 | c 复制 | m 移动","Shortcuts: n New file | N New dir | v View | p Properties | c Copy | m Move"));printf("%s\n",tr(c->language,"r 重命名 | d 删除 | x 权限 | h 隐藏文件 | g 位置 | ~ 主目录 | q 桌面","r Rename | d Delete | x Mode | h Hidden | g Locations | ~ Home | q Desktop"));}fflush(stdout);
}

static int fm_is_double_click(const char *path, char *last_path, size_t lastn, struct timespec *last_when) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
    long long elapsed_ms = (long long)(now.tv_sec - last_when->tv_sec) * 1000LL +
                           (long long)(now.tv_nsec - last_when->tv_nsec) / 1000000LL;
    int same = last_path[0] && strcmp(last_path, path) == 0 && elapsed_ms >= 0 && elapsed_ms <= 500;
    if (same) {
        last_path[0] = '\0';
        last_when->tv_sec = last_when->tv_nsec = 0;
        return 1;
    }
    snprintf(last_path, lastn, "%s", path);
    *last_when = now;
    return 0;
}

static int fm_context_menu(AppConfig *c, const char *path, const FmEntry *entry) {
    static const int actions[] = {'\r','p','c','m','r','d','x',0};
    const char *labels_zh[] = {entry && entry->type == FM_ENTRY_DIRECTORY ? "打开" : "打开/查看","属性","复制","移动","重命名","删除","修改权限","返回"};
    const char *labels_en[] = {entry && entry->type == FM_ENTRY_DIRECTORY ? "Open" : "Open/View","Properties","Copy","Move","Rename","Delete","Change mode","Back"};
    static const int parent_actions[] = {'\r',0};
    const char *parent_zh[] = {"打开上级目录","返回"};
    const char *parent_en[] = {"Open parent","Back"};
    size_t count = entry ? sizeof(actions)/sizeof(actions[0]) : sizeof(parent_actions)/sizeof(parent_actions[0]);
    size_t sel = 0;
    for (;;) {
        int w = terminal_width(); if (w > 78) w = 78;
        char safe[PATH_MAX]; fm_sanitize_text(path,safe,sizeof(safe));
        clear_screen(); printf("\033[?25lWorkbench %s | Workbench Files > %s\n",WB_VERSION,tr(c->language,"文件操作","File actions"));
        print_rule(w); printf("%s: ",tr(c->language,"目标","Target")); print_truncated(safe,w-9); putchar('\n');
        ui_hint_line(c->language,"操作：↑/↓  Enter  Esc/q 返回  鼠标可点击","Controls: Up/Down  Enter  Esc/q back  Mouse click"); print_rule(w);
        for (size_t i=0;i<count;i++) {
            const char *label = entry ? (c->language==LANG_ZH?labels_zh[i]:labels_en[i]) : (c->language==LANG_ZH?parent_zh[i]:parent_en[i]);
            if (i==sel) printf("\033[7m > "); else printf("   ");
            print_truncated(label,w-4); if(i==sel)printf("\033[0m"); putchar('\n');
        }
        fflush(stdout); int k=read_key();
        if(k==-1||k==27||k=='q'||k=='Q')return 0;
        if(k==KEY_UP||k=='k'||k=='K')sel=sel?sel-1:count-1;
        else if(k==KEY_DOWN||k=='j'||k=='J')sel=(sel+1)%count;
        else if(k==KEY_MOUSE&&!mouse_release&&mouse_button==0){int ro=mouse_y-6;if(ro>=0&&(size_t)ro<count){sel=(size_t)ro;return entry?actions[sel]:parent_actions[sel];}}
        else if(k=='\r'||k=='\n')return entry?actions[sel]:parent_actions[sel];
    }
}

static int fm_reload(const char *current, int show_hidden, FmDirectory *dir, AppConfig *c) {
    char err[256];
    fm_free_directory(dir);
    if (fm_load_directory(current, show_hidden, dir, err, sizeof(err)) != 0) {
        fm_show_error(c, "读取目录", "Read directory", err); return -1;
    }
    return 0;
}

static int fm_input_path_valid(const char *text) {
    if (!text || !*text) return 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p)
        if (*p < 0x20 || *p == 0x7f) return 0;
    return 1;
}

static void fm_create_item_ui(AppConfig *c, const char *current, int make_dir) {
    char name[NAME_MAX + 1], full[PATH_MAX], err[256];
    if (!fm_prompt_name(c,
                        make_dir ? "新建目录" : "新建文件",
                        make_dir ? "New directory" : "New file",
                        "输入名称：", "Enter name:", name, sizeof(name))) return;
    if (fm_join_path(current, name, full, sizeof(full)) != 0) {
        fm_show_error(c, make_dir ? "创建目录" : "创建文件",
                      make_dir ? "Create directory" : "Create file", strerror(errno));
        return;
    }
    int rc = make_dir ? fm_create_directory(full, 0755, err, sizeof(err))
                      : fm_create_file(full, 0644, err, sizeof(err));
    if (rc != 0) fm_show_error(c, make_dir ? "创建目录" : "创建文件",
                               make_dir ? "Create directory" : "Create file", err);
}

static void files_manager_menu(AppConfig *c) {
    char current[PATH_MAX];
    int show_hidden = 0;
    for (;;) {
        if (!fm_locations_menu(c, current, sizeof(current))) return;
        FmDirectory dir = {0};
        size_t sel = 0;
        int go_locations = 0;
        char last_click_path[PATH_MAX] = "";
        struct timespec last_click_when = {0,0};
        if (fm_reload(current, show_hidden, &dir, c) != 0) return;

        for (;;) {
        int has_parent = strcmp(current, "/") != 0;
        size_t total = dir.count + (size_t)has_parent;
        if (!total) sel = 0;
        else if (sel >= total) sel = total - 1;
        fm_draw_browser(c, current, &dir, show_hidden, sel);

        UiEvent ev = ui_read_event();
        int k = -2;
        int context_requested = 0;
        if (ev.kind == UI_EVENT_EOF) k = -1;
        else if (ev.kind == UI_EVENT_KEY) k = ev.key;
        else if (ev.kind == UI_EVENT_WHEEL_UP) k = KEY_UP;
        else if (ev.kind == UI_EVENT_WHEEL_DOWN) k = KEY_DOWN;
        else if (ev.kind == UI_EVENT_MOUSE_RELEASE) continue;
        else if (ev.kind == UI_EVENT_MOUSE_PRESS) {
            int hit = ui_hit_test(&ev);
            if (ev.button == 0 && hit == 10) k = 'g';
            else if (ev.button == 0 && hit == 11) k = '~';
            else if (ev.button == 0 && hit == 20) k = 'n';
            else if (ev.button == 0 && hit == 21) k = 'N';
            else if (ev.button == 0 && hit == 22) k = 'h';
            else if (ev.button == 0 && hit == 23) k = 'l';
            else if (ev.button == 0 && hit == 24) k = 'q';
            else {
                int ro = ev.y - fm_browser_row_start_y;
                if (ro < 0) continue;
                size_t click = fm_browser_visible_start + (size_t)ro;
                if (click >= fm_browser_visible_end || click >= total) continue;
                sel = click;
                char clicked_path[PATH_MAX]; const FmEntry *clicked_entry = NULL;
                if (fm_selected_path(current,&dir,has_parent,sel,clicked_path,sizeof(clicked_path),&clicked_entry) != 0) continue;
                if (ev.button == 0) {
                    if (fm_is_double_click(clicked_path,last_click_path,sizeof(last_click_path),&last_click_when)) k = '\r';
                    else continue;
                } else if (ev.button == 2) context_requested = 1;
                else continue;
            }
        }
        if (k == -1) { fm_free_directory(&dir); return; }
        if (k == 27 || k == 'q' || k == 'Q') { fm_free_directory(&dir); return; }
        if (k == 'g' || k == 'G') { go_locations = 1; break; }
        if (k == '~') {
            const char *home = getenv("HOME"); char resolved[PATH_MAX];
            if (home && realpath(home, resolved)) { strcpy(current, resolved); sel = 0; if (fm_reload(current, show_hidden, &dir, c) != 0) { fm_free_directory(&dir); return; } }
            else fm_show_error(c, "打开主目录", "Open home", strerror(errno));
            continue;
        }
        if (k == KEY_UP || k == 'k' || k == 'K') {
            if (total) sel = sel ? sel - 1 : total - 1;
            continue;
        }
        if (k == KEY_DOWN || k == 'j' || k == 'J') {
            if (total) sel = (sel + 1) % total;
            continue;
        }
        if (k == 'h' || k == 'H') {
            show_hidden = !show_hidden;
            sel = 0;
            if (fm_reload(current, show_hidden, &dir, c) != 0) break;
            continue;
        }
        if (k == 'l' || k == 'L') {
            c->language = c->language == LANG_ZH ? LANG_EN : LANG_ZH;
            save_config(c);
            continue;
        }
        if (k == KEY_LEFT || k == 127 || k == 8) {
            if (strcmp(current, "/") != 0) {
                char parent[PATH_MAX];
                if (fm_parent_path(current, parent, sizeof(parent)) == 0) {
                    strcpy(current, parent);
                    sel = 0;
                    if (fm_reload(current, show_hidden, &dir, c) != 0) break;
                }
            }
            continue;
        }
        if (k == 'n' || k == 'N') {
            fm_create_item_ui(c, current, k == 'N');
            sel = 0;
            if (fm_reload(current, show_hidden, &dir, c) != 0) break;
            continue;
        }
        if (!total) continue;

        char selected[PATH_MAX];
        const FmEntry *entry = NULL;
        if (fm_selected_path(current, &dir, has_parent, sel, selected, sizeof(selected), &entry) != 0) continue;

        if (context_requested) {
            int action = fm_context_menu(c, selected, entry);
            if (!action) continue;
            k = action;
        }

        if (k == '\r' || k == '\n') {
            struct stat target;
            if (stat(selected, &target) == 0 && S_ISDIR(target.st_mode)) {
                char resolved[PATH_MAX];
                if (realpath(selected, resolved)) {
                    strcpy(current, resolved);
                    sel = 0;
                    if (fm_reload(current, show_hidden, &dir, c) != 0) break;
                } else fm_show_error(c, "打开目录", "Open directory", strerror(errno));
            } else if (entry) {
                fm_view_file(c, selected);
            }
            continue;
        }
        if (!entry) continue;
        if (k == 'v' || k == 'V') { fm_view_file(c, selected); continue; }
        if (k == 'p' || k == 'P' || k == KEY_RIGHT) { fm_properties(c, selected); continue; }

        if (k == 'c' || k == 'C') {
            char input[PATH_MAX], dst[PATH_MAX], def[NAME_MAX + 16], err[256], safe_name[NAME_MAX + 1];
            fm_sanitize_text(entry->name, safe_name, sizeof(safe_name));
            snprintf(def, sizeof(def), "%s.copy", safe_name);
            if (prompt_text(c->language, tr(c->language, "复制", "Copy"),
                            "输入目标路径（相对路径以当前目录为基准）：",
                            "Destination path (relative to current directory):",
                            def, ARG_PATH, input, sizeof(input))) {
                if (!fm_input_path_valid(input)) {
                    show_message(c->language, "目标路径不能包含控制字符。", "Destination path cannot contain control characters.");
                } else if (fm_resolve_destination(current, input, dst, sizeof(dst)) != 0) {
                    fm_show_error(c, "复制", "Copy", strerror(errno));
                } else if (fm_copy_path(selected, dst, err, sizeof(err)) != 0) {
                    fm_show_error(c, "复制", "Copy", err);
                }
                if (fm_reload(current, show_hidden, &dir, c) != 0) break;
            }
            continue;
        }

        if (k == 'm' || k == 'M') {
            char input[PATH_MAX], dst[PATH_MAX], err[256];
            if (prompt_text(c->language, tr(c->language, "移动", "Move"),
                            "输入目标路径（相对路径以当前目录为基准）：",
                            "Destination path (relative to current directory):",
                            NULL, ARG_PATH, input, sizeof(input))) {
                if (!fm_input_path_valid(input)) {
                    show_message(c->language, "目标路径不能包含控制字符。", "Destination path cannot contain control characters.");
                } else if (fm_resolve_destination(current, input, dst, sizeof(dst)) != 0) {
                    fm_show_error(c, "移动", "Move", strerror(errno));
                } else if (fm_move_path(selected, dst, err, sizeof(err)) != 0) {
                    fm_show_error(c, "移动", "Move", err);
                }
                sel = 0;
                if (fm_reload(current, show_hidden, &dir, c) != 0) break;
            }
            continue;
        }

        if (k == 'r' || k == 'R') {
            char name[NAME_MAX + 1], dst[PATH_MAX], err[256];
            if (fm_prompt_name(c, "重命名", "Rename", "输入新名称：", "Enter new name:", name, sizeof(name))) {
                if (fm_join_path(current, name, dst, sizeof(dst)) != 0) {
                    fm_show_error(c, "重命名", "Rename", strerror(errno));
                } else if (fm_move_path(selected, dst, err, sizeof(err)) != 0) {
                    fm_show_error(c, "重命名", "Rename", err);
                }
                sel = 0;
                if (fm_reload(current, show_hidden, &dir, c) != 0) break;
            }
            continue;
        }

        if (k == 'd' || k == 'D') {
            char shown[NAME_MAX + 1], zh[512], en[512], err[256];
            fm_sanitize_text(entry->name, shown, sizeof(shown));
            int is_dir = entry->type == FM_ENTRY_DIRECTORY;
            snprintf(zh, sizeof(zh), is_dir ? "将递归删除目录「%s」及其中全部内容。确认删除？" : "确认删除「%s」？", shown);
            snprintf(en, sizeof(en), is_dir ? "Recursively delete directory \"%s\" and all contents?" : "Delete \"%s\"?", shown);
            if (confirm_line(c->language, zh, en)) {
                if (fm_remove_path(selected, is_dir, err, sizeof(err)) != 0) fm_show_error(c, "删除", "Delete", err);
                sel = 0;
                if (fm_reload(current, show_hidden, &dir, c) != 0) break;
            }
            continue;
        }

        if (k == 'x' || k == 'X') {
            if (entry->type == FM_ENTRY_SYMLINK) {
                show_message(c->language,
                             "为避免修改链接目标，当前版本不对符号链接执行 chmod。",
                             "To avoid changing a symlink target, chmod is disabled for symlinks in this version.");
                continue;
            }
            char def[8], text[16], err[256];
            mode_t mode;
            fm_mode_text(entry->mode, def);
            if (prompt_text(c->language, tr(c->language, "修改权限", "Change mode"),
                            "输入 3 或 4 位八进制权限（如 644 / 0755）：",
                            "Enter 3 or 4 octal digits (for example 644 / 0755):",
                            def, ARG_TEXT, text, sizeof(text))) {
                if (fm_parse_mode(text, &mode) != 0) {
                    show_message(c->language,
                                 "权限格式无效，只允许 3 或 4 位 0-7。",
                                 "Invalid mode; use exactly 3 or 4 digits from 0-7.");
                } else if (fm_chmod_path(selected, mode, err, sizeof(err)) != 0) {
                    fm_show_error(c, "修改权限", "Change mode", err);
                }
                if (fm_reload(current, show_hidden, &dir, c) != 0) break;
            }
            continue;
        }
        }
        fm_free_directory(&dir);
        if (!go_locations) return;
    }
}
