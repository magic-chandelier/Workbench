#!/bin/sh
set -eu
[ -f modules/files-manager/files_manager.c ]
[ -f modules/files-manager/files_manager.h ]
[ -f modules/files-manager/files_ui.inc ]
! grep -Eq '(^|[^[:alnum:]_])(system|popen|execl|execv|fork)[[:space:]]*\(' modules/files-manager/files_manager.c modules/files-manager/files_ui.inc
! grep -Fq '/bin/sh' modules/files-manager/files_manager.c modules/files-manager/files_ui.inc
grep -q 'lstat' modules/files-manager/files_manager.c
grep -q 'fm_destructive_path_allowed' modules/files-manager/files_manager.c
grep -q 'fm_remove_path' modules/files-manager/files_ui.inc
grep -q '/proc/self/mountinfo' modules/files-manager/files_manager.c
grep -q 'statvfs' modules/files-manager/files_manager.c
grep -q 'fs_type_is_pseudo' modules/files-manager/files_manager.c
grep -q 'KEY_MOUSE' modules/files-manager/files_ui.inc
grep -q 'fm_context_menu' modules/files-manager/files_ui.inc
