#!/bin/sh
set -eu
grep -q 'terminal_log_lines' main.c
grep -q 'terminal_load_lines' main.c
grep -q 'terminal_clear_on_open' main.c
grep -q 'terminal_log_lines=1000' main.c
grep -q 'terminal_load_lines=50' main.c
grep -q 'terminal_clear_on_open=0' main.c
grep -q '终端日志行数' main.c
grep -q '终端上滑加载' main.c
grep -q '重新打开终端时清屏' main.c
printf 'TERMINAL CONFIG PASS\n'
