#include "centos.h"

#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))
#define NOARG {NULL,NULL,NULL,ARG_NONE}
#define ARG(PZ,PE,D,K) {PZ,PE,D,K}
#define F(ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD) \
    {ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,"centos",{NOARG,NOARG,NOARG,NOARG}}
#define A1(ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,P1Z,P1E,D1,K1) \
    {ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,"centos",{ARG(P1Z,P1E,D1,K1),NOARG,NOARG,NOARG}}
#define A2(ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,P1Z,P1E,D1,K1,P2Z,P2E,D2,K2) \
    {ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,"centos",{ARG(P1Z,P1E,D1,K1),ARG(P2Z,P2E,D2,K2),NOARG,NOARG}}
#define A3(ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,P1Z,P1E,D1,K1,P2Z,P2E,D2,K2,P3Z,P3E,D3,K3) \
    {ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,"centos",{ARG(P1Z,P1E,D1,K1),ARG(P2Z,P2E,D2,K2),ARG(P3Z,P3E,D3,K3),NOARG}}
#define A4(ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,P1Z,P1E,D1,K1,P2Z,P2E,D2,K2,P3Z,P3E,D3,K3,P4Z,P4E,D4,K4) \
    {ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,"centos",{ARG(P1Z,P1E,D1,K1),ARG(P2Z,P2E,D2,K2),ARG(P3Z,P3E,D3,K3),ARG(P4Z,P4E,D4,K4)}}

static const Task CENTOS_TASKS[] = {
#include "actions/packages.inc"
#include "actions/services.inc"
#include "actions/firewall.inc"
#include "actions/selinux.inc"
#include "actions/network.inc"
#include "actions/system.inc"
};

const Module *centos_module(void) {
    static const Module module = {
        "centos",
        "CentOS 增强层",
        "CentOS Overlay",
        "CentOS Stream 9/10 专属操作",
        "CentOS Stream 9/10 specific operations",
        CENTOS_TASKS,
        ARRAY_LEN(CENTOS_TASKS)
    };
    return &module;
}
