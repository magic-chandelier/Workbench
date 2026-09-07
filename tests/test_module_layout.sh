#!/bin/sh
set -eu
[ -f workbench.h ]
[ -f modules/linux-core/linux_core.c ]
[ -f modules/linux-core/linux_core.h ]
[ -f modules/centos/centos.c ]
[ -f modules/centos/centos.h ]
[ -f modules/files-manager/files_manager.c ]
[ -f modules/files-manager/files_manager.h ]
[ -f modules/files-manager/files_ui.inc ]
! grep -q 'static const Task TASKS\[\]' main.c
grep -q 'linux_core_module' main.c
grep -q 'centos_module' main.c
grep -q 'modules/linux-core/linux_core.c' Makefile
grep -q 'modules/centos/centos.c' Makefile
grep -q 'modules/files-manager/files_manager.c' Makefile
out=$(./wb --self-test)
printf '%s\n' "$out" | grep -q 'profiles=2'
