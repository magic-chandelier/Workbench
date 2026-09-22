#include "linux_core.h"

#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))
#define NOARG {NULL,NULL,NULL,ARG_NONE}
#define ARG(PZ,PE,D,K) {PZ,PE,D,K}
#define F(ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD) \
    {ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,"linux-core",{NOARG,NOARG,NOARG,NOARG}}
#define A1(ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,P1Z,P1E,D1,K1) \
    {ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,"linux-core",{ARG(P1Z,P1E,D1,K1),NOARG,NOARG,NOARG}}
#define A2(ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,P1Z,P1E,D1,K1,P2Z,P2E,D2,K2) \
    {ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,"linux-core",{ARG(P1Z,P1E,D1,K1),ARG(P2Z,P2E,D2,K2),NOARG,NOARG}}
#define A3(ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,P1Z,P1E,D1,K1,P2Z,P2E,D2,K2,P3Z,P3E,D3,K3) \
    {ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,"linux-core",{ARG(P1Z,P1E,D1,K1),ARG(P2Z,P2E,D2,K2),ARG(P3Z,P3E,D3,K3),NOARG}}
#define A4(ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,P1Z,P1E,D1,K1,P2Z,P2E,D2,K2,P3Z,P3E,D3,K3,P4Z,P4E,D4,K4) \
    {ID,ZH,EN,DZH,DEN,KW,CAT,RISK,CMD,"linux-core",{ARG(P1Z,P1E,D1,K1),ARG(P2Z,P2E,D2,K2),ARG(P3Z,P3E,D3,K3),ARG(P4Z,P4E,D4,K4)}}

static const Task LINUX_CORE_TASKS[] = {
#include "actions/system.inc"
#include "actions/files.inc"
#include "actions/text.inc"
#include "actions/process.inc"
#include "actions/network.inc"
#include "actions/storage.inc"
#include "actions/permission.inc"
#include "actions/archive.inc"
#include "actions/user.inc"
};

const Module *linux_core_module(void) {
    static const Module module = {
        "linux-core",
        "Linux 通用指令集",
        "Linux Generic Command Set",
        "跨发行版 Linux 通用操作",
        "Cross-distro generic Linux operations",
        LINUX_CORE_TASKS,
        ARRAY_LEN(LINUX_CORE_TASKS)
    };
    return &module;
}
