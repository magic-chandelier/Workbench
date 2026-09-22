#!/bin/sh
set -eu

make package-only >/tmp/workbench-package-only.log
archive="dist/workbench-v0.16.1-dev4-source.tar.gz"
[ -f "$archive" ]

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

tar -xzf "$archive" -C "$tmp"
root="$tmp/workbench-v0.16.1-dev4"
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
[ -d "$root/modules/plugins" ]
[ -f "$root/modules/plugins/plugins.c" ]
[ -f "$root/modules/plugins/plugins.h" ]
[ -f "$root/modules/extensions/extensions.c" ]
[ -f "$root/modules/extensions/extensions.h" ]
[ -f "$root/include/version.h" ]
[ -f "$root/modules/command-sets/command_sets.c" ]
[ -f "$root/modules/command-sets/command_sets.h" ]
[ -f "$root/modules/terminal/terminal.c" ]
[ -f "$root/modules/terminal/terminal.h" ]
[ -d "$root/tests" ]
[ -d "$root/docs" ]
[ -f "$root/docs/PLUGIN_FORMAT.md" ]
[ -f "$root/examples/plugins/hello/plugin.wbp" ]
[ -f "$root/examples/plugins/hello/bin/hello" ]
[ -f "$root/examples/plugins/file-actions-demo/plugin.wbp" ]
[ -f "$root/examples/plugins/file-actions-demo/file-actions/show-path.wba" ]
[ ! -e "$root/wb" ]

if tar -tzf "$archive" | grep -Eq '(^|/)wb$'; then
    echo "compiled wb must not be present in source archive" >&2
    exit 1
fi

if tar -tzf "$archive" | grep -Eq '(^|/)([^/]+\.o|[^/]+\.a|[^/]+\.so|[^/]+\.pyc|__pycache__|wb)$'; then
    echo "compiled artifacts must not be present in source archive" >&2
    exit 1
fi
