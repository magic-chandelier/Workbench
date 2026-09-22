#!/bin/sh
set -eu

# The generic plugin core must not accumulate per-product compatibility branches.
if grep -Ein '\b(docker|7z|7zz|nginx|podman|mysql|postgres|redis)\b' main.c modules/plugins/plugins.c modules/plugins/plugins.h modules/extensions/extensions.c modules/extensions/extensions.h >/tmp/wb-plugin-boundary.out; then
    cat /tmp/wb-plugin-boundary.out >&2
    echo 'plugin-specific product logic found in Workbench core' >&2
    exit 1
fi

# v0.2 deliberately executes plugins out-of-process rather than loading .so code into Core.
if grep -Ein '\b(dlopen|dlmopen|dlsym)\s*\(' main.c modules/plugins/plugins.c modules/plugins/plugins.h modules/extensions/extensions.c modules/extensions/extensions.h >/tmp/wb-plugin-dlopen.out; then
    cat /tmp/wb-plugin-dlopen.out >&2
    echo 'in-process dynamic plugin loading is forbidden in Plugin System v0.2' >&2
    exit 1
fi

echo 'PLUGIN CORE BOUNDARY PASS'
