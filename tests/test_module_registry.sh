#!/bin/sh
set -eu
grep -q 'module_count' main.c
grep -q 'module_at' main.c
grep -q 'command_sets_menu' main.c
grep -q 'module_command_set_browser' main.c
grep -q 'COMMAND_MODE_EXECUTE' main.c
grep -q 'COMMAND_MODE_INSERT' main.c
! grep -q 'terminal_insert_task_picker' main.c
! grep -q 'sel=(sel+1)%3' main.c
! grep -q 'ro>=0&&ro<3' main.c
