#!/bin/sh
set -eu

if [ "$(id -u)" -ne 0 ]; then
    set +e
    ./wb --plugin-remove-system demo >/tmp/wb-system-plugin-remove.out 2>/tmp/wb-system-plugin-remove.err
    rc=$?
    set -e
    [ "$rc" -eq 3 ]
    grep -q 'requires root' /tmp/wb-system-plugin-remove.err
    echo 'SYSTEM PLUGIN REMOVE PASS (non-root refusal)'
    exit 0
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
root="$tmp/plugins"
mkdir -p "$root/demo/sub"
printf 'keep\n' > "$tmp/outside"
ln -s "$tmp/outside" "$root/demo/sub/outside-link"

WB_ALLOW_TEST_ROOTS=1 WB_PLUGIN_SYSTEM_ROOT="$root" ./wb --plugin-remove-system demo
[ ! -e "$root/demo" ]
[ -f "$tmp/outside" ]

mkdir -p "$root/safe"
printf 'sentinel\n' > "$tmp/sentinel"
set +e
WB_ALLOW_TEST_ROOTS=1 WB_PLUGIN_SYSTEM_ROOT="$root" ./wb --plugin-remove-system '../safe' >/dev/null 2>"$tmp/err"
rc=$?
set -e
[ "$rc" -ne 0 ]
[ -d "$root/safe" ]
[ -f "$tmp/sentinel" ]

echo 'SYSTEM PLUGIN REMOVE PASS'
