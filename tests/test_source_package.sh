#!/bin/sh
set -eu

make package-only >/tmp/workbench-package-only.log
archive="dist/workbench-v0.15.0-source.tar.gz"
[ -f "$archive" ]

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

tar -xzf "$archive" -C "$tmp"
root="$tmp/workbench-v0.15.0"
[ -d "$root" ]
[ -f "$root/main.c" ]
[ -f "$root/Makefile" ]
[ -f "$root/LICENSE" ]
[ -f "$root/THIRD_PARTY_NOTICES.md" ]
[ -f "$root/DEPENDENCY_POLICY.md" ]
[ -f "$root/README.md" ]
[ -f "$root/CHANGELOG.md" ]
[ -d "$root/modules" ]
[ -d "$root/modules/terminal" ]
[ -d "$root/modules/command-sets" ]
[ -f "$root/modules/command-sets/command_sets.c" ]
[ -f "$root/modules/command-sets/command_sets.h" ]
[ -f "$root/modules/terminal/terminal.c" ]
[ -f "$root/modules/terminal/terminal.h" ]
[ -d "$root/tests" ]
[ -d "$root/docs" ]
[ ! -e "$root/wb" ]

if tar -tzf "$archive" | grep -Eq '(^|/)wb$'; then
    echo "compiled wb must not be present in source archive" >&2
    exit 1
fi
