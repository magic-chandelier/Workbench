#!/bin/sh
set -eu
grep -q '#define WB_VERSION "0.15.0"' main.c
grep -q '^VERSION := 0.15.0$' Makefile
grep -q '^test:' Makefile
grep -q '^package:' Makefile
grep -q '^# Workbench v0.15.0$' README.md
grep -q '374' README.md
grep -q '483' README.md
grep -q 'CentOS' README.md
grep -q 'modules/linux-core/actions/' README.md
grep -q 'modules/centos/actions/' README.md
grep -q '^## v0.15.0：Command Sets 指令集容器' README.md
grep -q 'Keyboard-only' README.md
grep -q 'Mouse-only' README.md
grep -q '虚拟键盘' README.md
grep -q '^## Workbench Files' README.md
grep -q 'modules/files-manager/' README.md
grep -q 'Locations' README.md
grep -q '/proc/self/mountinfo' README.md
grep -q 'statvfs' README.md
grep -q '^## 0.15.0' CHANGELOG.md

grep -q 'virtual_keyboard' main.c
grep -q 'shortcut_hints' main.c
grep -q '快捷键提示' README.md

grep -q 'workbench-v0.15.0-source.tar.gz' README.md
grep -q 'Workbench Source License 1.0' README.md
grep -q 'THIRD_PARTY_NOTICES.md' README.md
grep -q 'DEPENDENCY_POLICY.md' README.md

grep -q '^## Workbench Terminal' README.md
grep -q '1000' README.md
grep -q '50' README.md
grep -q 'terminal_clear_on_open' README.md
grep -q 'modules/terminal/' README.md
grep -q 'PTY' README.md
grep -q 'Ctrl+]' README.md
grep -q '^## 0.15.0' CHANGELOG.md

grep -q '^## Command Sets' README.md
grep -q 'Linux Commands.*System.*Read-only' README.md
grep -q '~/.local/share/workbench/command-sets/' README.md
grep -q '\.wbc' README.md
grep -q '64' README.md
grep -q '256' README.md
grep -q 'INSERT' README.md
grep -q 'modules/command-sets/' README.md
