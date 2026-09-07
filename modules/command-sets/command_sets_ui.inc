static const char *command_set_name(const CommandSet *set,Language l){return l==LANG_ZH?set->name_zh:set->name_en;}
static const char *command_set_desc(const CommandSet *set,Language l){if(set->source==COMMAND_SET_SYSTEM)return effective_workbench_desc(l);return l==LANG_ZH?set->desc_zh:set->desc_en;}
static unsigned long g_command_id_counter=0;

static int command_ui_make_id(const char *prefix,char *out,size_t cap){
    for(int tries=0;tries<1000;tries++){
        unsigned long v=++g_command_id_counter;
        int n=snprintf(out,cap,"%s-%ld-%lu",prefix,(long)getpid(),v);
        if(n<0||(size_t)n>=cap)return -1;
        if(!wb_command_sets_find(&g_command_sets,out))return 0;
    }
    return -1;
}

static int command_ui_make_action_id(const CommandSet *set,char *out,size_t cap){
    for(int tries=0;tries<1000;tries++){
        unsigned long v=++g_command_id_counter;
        int n=snprintf(out,cap,"cmd-%ld-%lu",(long)getpid(),v);
        if(n<0||(size_t)n>=cap)return -1;
        if(!wb_command_action_find(set,out))return 0;
    }
    return -1;
}

static int command_ui_has_placeholder(const char *cmd,int n){char p[4]={'{',(char)('0'+n),'}','\0'};return cmd&&strstr(cmd,p)!=NULL;}

static int command_ui_select_risk(AppConfig *c,Risk initial,Risk *out){
    size_t sel=(size_t)initial;
    for(;;){Language l=c->language;int w=terminal_width();if(w>80)w=80;clear_screen();ui_hits_reset();printf("\033[?25lWorkbench %s | %s\n",WB_VERSION,tr(l,"风险等级","Risk"));print_rule(w);ui_hint_line(l,"↑/↓ 选择，Enter 确定，Esc/q 取消。鼠标可点击。","Up/Down select, Enter accept, Esc/q cancel. Mouse can click.");putchar('\n');
        for(size_t i=0;i<4;i++){const char *label=i<3?risk_name((Risk)i,l):tr(l,"取消","Cancel");ui_hit_add(100+(int)i,1,5+(int)i,w,5+(int)i);if(i==sel)printf("\033[7m > ");else printf("   ");printf("%s",label);if(i==sel)printf("\033[0m");putchar('\n');}fflush(stdout);
        UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)return 0;int activate=0;
        if(ev.kind==UI_EVENT_KEY){int k=ev.key;if(k==27||k=='q'||k=='Q')return 0;if(k==KEY_UP||k=='k'||k=='K')sel=sel?sel-1:3;else if(k==KEY_DOWN||k=='j'||k=='J')sel=(sel+1)%4;else if(k=='\r'||k=='\n')activate=1;}
        else if(ev.kind==UI_EVENT_WHEEL_UP)sel=sel?sel-1:3;else if(ev.kind==UI_EVENT_WHEEL_DOWN)sel=(sel+1)%4;else if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0){int hit=ui_hit_test(&ev);if(hit>=100&&hit<104){sel=(size_t)(hit-100);activate=1;}}
        if(activate){if(sel==3)return 0;*out=(Risk)sel;return 1;}
    }
}

static const char *command_ui_arg_kind_name(ArgKind k,Language l){
    switch(k){
        case ARG_TEXT:return tr(l,"文本","Text");case ARG_PATH:return tr(l,"路径","Path");case ARG_DESTRUCTIVE_PATH:return tr(l,"破坏性路径","Destructive path");
        case ARG_UINT:return tr(l,"正整数","Positive integer");case ARG_PORT:return tr(l,"端口","Port");case ARG_PID:return "PID";default:return tr(l,"无","None");
    }
}

static int command_ui_select_arg_kind(AppConfig *c,ArgKind initial,ArgKind *out){
    const ArgKind kinds[]={ARG_TEXT,ARG_PATH,ARG_DESTRUCTIVE_PATH,ARG_UINT,ARG_PORT,ARG_PID};
    size_t sel=0;for(size_t i=0;i<ARRAY_LEN(kinds);i++)if(kinds[i]==initial)sel=i;
    for(;;){Language l=c->language;int w=terminal_width();if(w>86)w=86;clear_screen();ui_hits_reset();printf("\033[?25lWorkbench %s | %s\n",WB_VERSION,tr(l,"参数类型","Parameter type"));print_rule(w);ui_hint_line(l,"↑/↓ 选择，Enter 确定，Esc/q 取消。鼠标可点击。","Up/Down select, Enter accept, Esc/q cancel. Mouse can click.");putchar('\n');
        for(size_t i=0;i<ARRAY_LEN(kinds)+1;i++){const char *label=i<ARRAY_LEN(kinds)?command_ui_arg_kind_name(kinds[i],l):tr(l,"取消","Cancel");ui_hit_add(100+(int)i,1,5+(int)i,w,5+(int)i);if(i==sel)printf("\033[7m > ");else printf("   ");printf("%s",label);if(i==sel)printf("\033[0m");putchar('\n');}fflush(stdout);
        UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)return 0;int activate=0;
        if(ev.kind==UI_EVENT_KEY){int k=ev.key;if(k==27||k=='q'||k=='Q')return 0;if(k==KEY_UP||k=='k'||k=='K')sel=sel?sel-1:ARRAY_LEN(kinds);else if(k==KEY_DOWN||k=='j'||k=='J')sel=(sel+1)%(ARRAY_LEN(kinds)+1);else if(k=='\r'||k=='\n')activate=1;}
        else if(ev.kind==UI_EVENT_WHEEL_UP)sel=sel?sel-1:ARRAY_LEN(kinds);else if(ev.kind==UI_EVENT_WHEEL_DOWN)sel=(sel+1)%(ARRAY_LEN(kinds)+1);else if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0){int hit=ui_hit_test(&ev);if(hit>=100&&hit<100+(int)ARRAY_LEN(kinds)+1){sel=(size_t)(hit-100);activate=1;}}
        if(activate){if(sel==ARRAY_LEN(kinds))return 0;*out=kinds[sel];return 1;}
    }
}

static int command_ui_edit_set(AppConfig *c,CommandSet *set){
    if(!set||!wb_command_set_can_edit(set))return 0;
    Language l=c->language;char name[128],desc[256];
    if(!ui_input_dialog(l,tr(l,"编辑指令集","Edit Command Set"),tr(l,"指令集名称","Command set name"),command_set_name(set,l),name,sizeof(name),0))return 0;
    if(!ui_input_dialog(l,tr(l,"编辑指令集","Edit Command Set"),tr(l,"说明","Description"),command_set_desc(set,l),desc,sizeof(desc),1))return 0;
    if(wb_command_set_update_metadata(&g_command_sets,set,name,name,desc,desc)){show_message(l,"保存指令集失败。","Failed to save command set.");return 0;}return 1;
}

static int command_ui_create_set(AppConfig *c,size_t *selected){
    Language l=c->language;if(g_command_sets.count>=WB_MAX_USER_COMMAND_SETS+1){show_message(l,"自定义指令集数量已达到上限。","The custom command-set limit has been reached.");return 0;}
    char name[128],desc[256],id[64];if(!ui_input_dialog(l,tr(l,"新建指令集","New Command Set"),tr(l,"指令集名称","Command set name"),NULL,name,sizeof(name),0))return 0;
    if(!ui_input_dialog(l,tr(l,"新建指令集","New Command Set"),tr(l,"说明","Description"),NULL,desc,sizeof(desc),1))return 0;
    if(command_ui_make_id("user",id,sizeof(id))){show_message(l,"无法生成指令集 ID。","Could not generate a command-set ID.");return 0;}
    CommandSet *set=NULL;if(wb_command_set_create_user(&g_command_sets,id,name,name,desc,&set)){show_message(l,"创建指令集失败。","Failed to create command set.");return 0;}
    if(selected)*selected=(size_t)(set-g_command_sets.sets);
    return 1;
}

static int command_ui_delete_set(AppConfig *c,CommandSet *set){
    if(!set||!wb_command_set_can_edit(set)){show_message(c->language,"系统预制指令集不能删除。","System command sets cannot be deleted.");return 0;}
    if(!confirm_line(c->language,"确认删除这个自定义指令集及其中全部指令？","Delete this custom command set and all of its commands?"))return 0;
    if(wb_command_set_delete_user(&g_command_sets,set)){show_message(c->language,"删除指令集失败。","Failed to delete command set.");return 0;}return 1;
}

static int command_ui_edit_action(AppConfig *c,CommandSet *set,const Task *existing){
    if(!set||!wb_command_set_can_edit(set))return 0;
    Language l=c->language;
    char id[64],name[128],desc[256],command[WB_INPUT],keywords[256];
    char prompt_zh[WB_MAX_ARGS][128]={{0}},prompt_en[WB_MAX_ARGS][128]={{0}},defaults[WB_MAX_ARGS][256]={{0}};TaskArg args[WB_MAX_ARGS]={{0}};
    if(existing){if(strlen(existing->id)>=sizeof(id))return 0;strcpy(id,existing->id);}else if(command_ui_make_action_id(set,id,sizeof(id)))return 0;
    if(!ui_input_dialog(l,tr(l,existing?"编辑指令":"添加指令",existing?"Edit Command":"Add Command"),tr(l,"指令名称","Command name"),existing?task_title(existing,l):NULL,name,sizeof(name),0))return 0;
    if(!ui_input_dialog(l,tr(l,existing?"编辑指令":"添加指令",existing?"Edit Command":"Add Command"),tr(l,"说明","Description"),existing?task_desc(existing,l):NULL,desc,sizeof(desc),1))return 0;
    if(!ui_input_dialog(l,tr(l,existing?"编辑指令":"添加指令",existing?"Edit Command":"Add Command"),tr(l,"命令模板，例如 ping {1}","Command template, for example ping {1}"),existing?existing->command:NULL,command,sizeof(command),0))return 0;
    if(!ui_input_dialog(l,tr(l,existing?"编辑指令":"添加指令",existing?"Edit Command":"Add Command"),tr(l,"搜索关键词","Search keywords"),existing?existing->keywords:NULL,keywords,sizeof(keywords),1))return 0;
    Risk risk=existing?existing->risk:RISK_NORMAL;if(!command_ui_select_risk(c,risk,&risk))return 0;
    for(int i=0;i<WB_MAX_ARGS;i++){
        if(!command_ui_has_placeholder(command,i+1)){args[i].kind=ARG_NONE;continue;}
        ArgKind kind=existing&&existing->args[i].kind!=ARG_NONE?existing->args[i].kind:ARG_TEXT;if(!command_ui_select_arg_kind(c,kind,&kind))return 0;
        char label[128];snprintf(label,sizeof(label),l==LANG_ZH?"参数 %d 提示":"Parameter %d prompt",i+1);
        const char *oldp=existing&&existing->args[i].kind!=ARG_NONE?tr(l,existing->args[i].prompt_zh,existing->args[i].prompt_en):NULL;
        if(!ui_input_dialog(l,tr(l,"配置参数","Configure Parameter"),label,oldp,prompt_zh[i],sizeof(prompt_zh[i]),0))return 0;
        snprintf(prompt_en[i],sizeof(prompt_en[i]),"%s",prompt_zh[i]);
        snprintf(label,sizeof(label),l==LANG_ZH?"参数 %d 默认值（可留空）":"Parameter %d default (may be empty)",i+1);
        const char *oldd=existing&&existing->args[i].kind!=ARG_NONE?existing->args[i].default_value:NULL;
        if(!ui_input_dialog(l,tr(l,"配置参数","Configure Parameter"),label,oldd,defaults[i],sizeof(defaults[i]),1))return 0;
        args[i]=(TaskArg){prompt_zh[i],prompt_en[i],defaults[i],kind};
    }
    Task t={id,name,name,desc,desc,keywords,CAT_ALL,risk,command,"user",{{0}}};for(int i=0;i<WB_MAX_ARGS;i++)t.args[i]=args[i];
    int rc=existing?wb_command_action_update(&g_command_sets,set,existing->id,&t):wb_command_action_add(&g_command_sets,set,&t,NULL);
    if(rc){show_message(l,"保存自定义指令失败。请检查命令中的 {1}..{4} 参数是否与配置一致。","Failed to save the custom command. Check that {1}..{4} placeholders match their parameter definitions.");return 0;}return 1;
}

static size_t command_ui_collect_user_matches(const CommandSet *set,Language l,const char *q,size_t *out,size_t cap){
    size_t n=0,tc=wb_command_set_task_count(set);for(size_t i=0;i<tc&&n<cap;i++){const Task*t=wb_command_set_task_at(set,i);if(t&&task_matches(t,l,CAT_ALL,q))out[n++]=i;}return n;
}

static void command_ui_draw_user_set(CommandSet *set,AppConfig*c,size_t sel,const char*q,const size_t*m,size_t mc,CommandBrowserMode mode){
    Language l=c->language;int w=terminal_width(),h=terminal_height();if(w>118)w=118;if(h<18)h=18;clear_screen();ui_hits_reset();printf("\033[?25lWorkbench %s | %s > %s\n",WB_VERSION,tr(l,"指令集","Command Sets"),command_set_name(set,l));print_rule(w);
    ui_hint_line(l,"键盘：Enter 执行/插入，a 添加，e 编辑指令，d 删除指令，m 编辑指令集，D 删除指令集，/ 搜索，q 返回。","Keyboard: Enter run/insert, a add, e edit command, d delete command, m edit set, D delete set, / search, q back.");
    int x=1;ui_print_button(10,4,&x,tr(l,"搜索","Search"));ui_print_button(11,4,&x,tr(l,"添加指令","Add Command"));ui_print_button(12,4,&x,tr(l,"编辑指令集","Edit Set"));ui_print_button(13,4,&x,tr(l,"删除指令集","Delete Set"));ui_print_button(14,4,&x,tr(l,"返回","Back"));putchar('\n');
    printf("%s: %s   %s: %s\n",tr(l,"说明","Description"),command_set_desc(set,l),tr(l,"搜索","Search"),*q?q:tr(l,"（无）","(none)"));print_rule(w);
    int vis=h-11;if(vis<4)vis=4;size_t st=0;if(mc>(size_t)vis&&sel>=(size_t)vis)st=sel-(size_t)vis+1;size_t en=st+(size_t)vis;if(en>mc)en=mc;
    if(!mc)printf("  %s\n",tr(l,"没有指令。使用 [添加指令] 创建。","No commands. Use [Add Command] to create one."));
    for(size_t r=st;r<en;r++){const Task*t=wb_command_set_task_at(set,m[r]);int y=7+(int)(r-st);ui_hit_add(10000+(int)r,1,y,w,y);ui_hit_add(20000+(int)r,39,y,48,y);ui_hit_add(30000+(int)r,50,y,59,y);ui_hit_add(40000+(int)r,61,y,69,y);ui_hit_add(50000+(int)r,71,y,80,y);
        if(r==sel)printf("\033[7m > ");else printf("   ");print_cell(task_title(t,l),22);print_cell(risk_name(t->risk,l),10);putchar(' ');print_cell(mode==COMMAND_MODE_EXECUTE?tr(l,"[执行]","[Run]"):tr(l,"[插入]","[Insert]"),10);putchar(' ');print_cell(tr(l,"[属性]","[Props]"),10);putchar(' ');print_cell(tr(l,"[编辑]","[Edit]"),9);putchar(' ');print_cell(tr(l,"[删除]","[Delete]"),10);if(r==sel)printf("\033[0m");putchar('\n');}
    putchar('\n');print_rule(w);printf("%s: %zu\n",tr(l,"匹配指令","Matching commands"),mc);fflush(stdout);
}

static int command_ui_user_set_menu(CommandSet *set,AppConfig*c,CommandBrowserMode mode,char*out,size_t cap){
    size_t sel=0,m[WB_MAX_USER_ACTIONS];char q[MAX_QUERY]="";
    for(;;){size_t mc=command_ui_collect_user_matches(set,c->language,q,m,ARRAY_LEN(m));if(mc&&sel>=mc)sel=mc-1;else if(!mc)sel=0;command_ui_draw_user_set(set,c,sel,q,m,mc,mode);UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)return 0;int action=0,activate=0,props=0,edit=0,del=0;
        if(ev.kind==UI_EVENT_KEY){int k=ev.key;if(k==27||k=='q'||k=='Q'||k==KEY_LEFT)return 0;if(k==KEY_UP||k=='k'||k=='K'){if(mc)sel=sel?sel-1:mc-1;}else if(k==KEY_DOWN||k=='j'||k=='J'){if(mc)sel=(sel+1)%mc;}else if(k=='/'||k=='s'||k=='S')action=10;else if(k=='a'||k=='A')action=11;else if(k=='m'||k=='M')action=12;else if(k=='D')action=13;else if(k=='e'||k=='E')edit=mc>0;else if(k=='d')del=mc>0;else if(k=='p'||k=='P'||k==KEY_RIGHT)props=mc>0;else if((k=='\r'||k=='\n')&&mc)activate=1;}
        else if(ev.kind==UI_EVENT_WHEEL_UP){if(mc)sel=sel?sel-1:mc-1;}else if(ev.kind==UI_EVENT_WHEEL_DOWN){if(mc)sel=(sel+1)%mc;}
        else if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0){int hit=ui_hit_test(&ev);if(hit>=50000){size_t r=(size_t)(hit-50000);if(r<mc){sel=r;del=1;}}else if(hit>=40000){size_t r=(size_t)(hit-40000);if(r<mc){sel=r;edit=1;}}else if(hit>=30000){size_t r=(size_t)(hit-30000);if(r<mc){sel=r;props=1;}}else if(hit>=20000){size_t r=(size_t)(hit-20000);if(r<mc){sel=r;activate=1;}}else if(hit>=10000){size_t r=(size_t)(hit-10000);if(r<mc)sel=r;}else action=hit;}
        if(action==14)return 0;
        if(action==10){char next[MAX_QUERY];if(ui_input_dialog(c->language,tr(c->language,"搜索指令","Search Commands"),tr(c->language,"输入标题、说明、关键词、命令或 ID。","Enter title, description, keyword, command, or ID."),q,next,sizeof(next),1)){snprintf(q,sizeof(q),"%s",next);sel=0;}continue;}
        if(action==11){command_ui_edit_action(c,set,NULL);continue;}
        if(action==12){command_ui_edit_set(c,set);continue;}
        if(action==13){if(command_ui_delete_set(c,set))return 2;continue;}
        if(mc&&(activate||props||edit||del)){const Task*t=wb_command_set_task_at(set,m[sel]);if(!t)continue;
            if(activate){if(browser_activate_task(t,c,mode,out,cap))return 1;}
            else if(props){if(show_task_detail_browser(t,c,mode,out,cap))return 1;}
            else if(edit){command_ui_edit_action(c,set,t);}
            else if(del&&confirm_line(c->language,"确认删除这条自定义指令？","Delete this custom command?")){char id[64];snprintf(id,sizeof(id),"%s",t->id);if(wb_command_action_delete(&g_command_sets,set,id))show_message(c->language,"删除指令失败。","Failed to delete command.");}
        }
    }
}

static void draw_command_sets_menu(size_t sel,AppConfig *c){
    Language l=c->language;int w=terminal_width();if(w>110)w=110;clear_screen();ui_hits_reset();
    printf("\033[?25lWorkbench %s | %s\n",WB_VERSION,tr(l,"指令集","Command Sets"));print_rule(w);
    ui_hint_line(l,"键盘：↑/↓ 选择，Enter 打开，n 新建，e 编辑，d 删除，Esc/q 返回。鼠标可点击。","Keyboard: Up/Down select, Enter open, n new, e edit, d delete, Esc/q back. Mouse can click.");
    int x=1;ui_print_button(10,4,&x,tr(l,"新建指令集","New Command Set"));ui_print_button(11,4,&x,tr(l,"编辑指令集","Edit Set"));ui_print_button(12,4,&x,tr(l,"删除指令集","Delete Set"));ui_print_button(13,4,&x,tr(l,"返回","Back"));putchar('\n');print_rule(w);
    for(size_t i=0;i<g_command_sets.count;i++){
        const CommandSet *set=&g_command_sets.sets[i];int y=6+(int)i;ui_hit_add(100+(int)i,1,y,w,y);
        if(i==sel)printf("\033[7m > ");else printf("   ");print_cell(command_set_name(set,l),28);
        if(set->source==COMMAND_SET_SYSTEM)printf("[System] [Read-only]  ");else printf("[Custom]              ");
        printf("%4zu  ",wb_command_set_task_count(set));print_truncated(command_set_desc(set,l),w-62);if(i==sel)printf("\033[0m");putchar('\n');
    }
    putchar('\n');print_rule(w);printf("%s: %zu\n",tr(l,"指令集","Command sets"),g_command_sets.count);fflush(stdout);
}

static int command_sets_menu(AppConfig *c, CommandBrowserMode mode, char *insert_out, size_t insert_cap){
    size_t sel=0;
    for(;;){
        if(g_command_sets.count&&sel>=g_command_sets.count)sel=g_command_sets.count-1;
        draw_command_sets_menu(sel,c);UiEvent ev=ui_read_event();if(ev.kind==UI_EVENT_EOF)return 0;int activate=0,action=0;
        if(ev.kind==UI_EVENT_KEY){int k=ev.key;if(k==27||k=='q'||k=='Q'||k==KEY_LEFT)return 0;if(k==KEY_UP||k=='k'||k=='K'){if(g_command_sets.count)sel=sel?sel-1:g_command_sets.count-1;}else if(k==KEY_DOWN||k=='j'||k=='J'){if(g_command_sets.count)sel=(sel+1)%g_command_sets.count;}else if(k=='n'||k=='N')action=10;else if(k=='e'||k=='E')action=11;else if(k=='d'||k=='D')action=12;else if((k=='\r'||k=='\n')&&g_command_sets.count)activate=1;}
        else if(ev.kind==UI_EVENT_WHEEL_UP){if(g_command_sets.count)sel=sel?sel-1:g_command_sets.count-1;}else if(ev.kind==UI_EVENT_WHEEL_DOWN){if(g_command_sets.count)sel=(sel+1)%g_command_sets.count;}
        else if(ev.kind==UI_EVENT_MOUSE_PRESS&&ev.button==0){int hit=ui_hit_test(&ev);if(hit>=100&&hit<100+(int)g_command_sets.count){sel=(size_t)(hit-100);activate=1;}else action=hit;}
        if(action==13)return 0;
        if(action==10){command_ui_create_set(c,&sel);continue;}
        if(action==11){if(g_command_sets.count){CommandSet*set=&g_command_sets.sets[sel];if(!wb_command_set_can_edit(set))show_message(c->language,"系统预制指令集不能修改。","System command sets cannot be modified.");else command_ui_edit_set(c,set);}continue;}
        if(action==12){if(g_command_sets.count){CommandSet*set=&g_command_sets.sets[sel];if(command_ui_delete_set(c,set)&&sel>=g_command_sets.count&&sel)sel--;}continue;}
        if(activate){CommandSet *set=&g_command_sets.sets[sel];if(set->source==COMMAND_SET_SYSTEM){if(module_command_set_browser(c,module_at(0),mode,insert_out,insert_cap))return 1;}else{int r=command_ui_user_set_menu(set,c,mode,insert_out,insert_cap);if(r==1)return 1;if(r==2&&sel>=g_command_sets.count&&sel)sel--;}}
    }
}
