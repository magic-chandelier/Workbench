#!/bin/sh
set -eu
grep -q '#define WB_VERSION_STRING "0.16.1-dev4"' include/version.h
grep -q '^VERSION := 0.16.1-dev4$' Makefile
grep -q '^test:' Makefile
grep -q '^package:' Makefile
grep -q '^# Workbench v0.16.1-dev4$' README.md
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
grep -q '^## 0.16.1-dev4' CHANGELOG.md
grep -q '^## 0.16.1-dev1' CHANGELOG.md
grep -q '^## 0.16.0' CHANGELOG.md
grep -q '^## 0.15.0' CHANGELOG.md

grep -q 'virtual_keyboard' main.c
grep -q 'shortcut_hints' main.c
grep -q '快捷键提示' README.md

grep -q 'workbench-v0.16.1-dev4-source.tar.gz' README.md
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
grep -q '^## 0.16.0' CHANGELOG.md
grep -q '^## 0.15.0' CHANGELOG.md

grep -q '^## Command Sets' README.md
grep -q 'Linux Commands.*System.*Read-only' README.md
grep -q '~/.local/share/workbench/command-sets/' README.md
grep -q '\.wbc' README.md
grep -q '64' README.md
grep -q '256' README.md
grep -q 'INSERT' README.md
grep -q 'modules/command-sets/' README.md

grep -q '^## v0.16.0：Plugin System v0.1.0' README.md
grep -q '/usr/local/share/workbench/plugins/' README.md
grep -q '~/.local/share/workbench/plugins/' README.md
grep -q 'Applications' README.md
grep -q 'Settings.*Plugins\|Plugins.*Settings' README.md
grep -q 'Plugin.*Read-only' README.md
grep -q 'Plugin System v0.1.0' README.md
grep -q 'plugins_guard=PASS' README.md
grep -q 'modules/plugins/' README.md
grep -q 'docs/PLUGIN_FORMAT.md' README.md
[ -f docs/PLUGIN_FORMAT.md ]
[ -f examples/plugins/hello/plugin.wbp ]
[ -f examples/plugins/hello/bin/hello ]
[ -f examples/plugins/file-actions-demo/plugin.wbp ]
[ -f examples/plugins/file-actions-demo/file-actions/show-path.wba ]

grep -q 'Enable / Disable' docs/PLUGIN_FORMAT.md
[ -f include/version.h ]
[ -f modules/extensions/extensions.c ]
[ -f modules/extensions/extensions.h ]
[ -f docs/EXTENSION_API.md ]
