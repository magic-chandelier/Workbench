#!/bin/sh
set -eu
root=$(pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/.config/workbench"

# Detection is advisory only: fresh installs remain Generic even on CentOS.
out=$(HOME="$tmp" WB_OS_RELEASE="$root/tests/fixtures/os-release-centos" ./wb --profile-info)
printf '%s\n' "$out" | grep -q 'configured=generic'
printf '%s\n' "$out" | grep -q 'detected=centos'
printf '%s\n' "$out" | grep -q 'detected_version=10'
printf '%s\n' "$out" | grep -q 'effective=generic'

# Legacy v0.9.0 auto config must fail safe to Generic, never auto-enable CentOS.
cat > "$tmp/.config/workbench/config" <<'CFG'
language=zh
confirm_normal=0
confirm_sensitive=0
confirm_privileged=1
linux_profile=auto
CFG
out=$(HOME="$tmp" WB_OS_RELEASE="$root/tests/fixtures/os-release-centos" ./wb --profile-info)
printf '%s\n' "$out" | grep -q 'configured=generic'
printf '%s\n' "$out" | grep -q 'detected=centos'
printf '%s\n' "$out" | grep -q 'effective=generic'

# CentOS is enabled only by explicit user selection.
cat > "$tmp/.config/workbench/config" <<'CFG'
linux_profile=centos
CFG
out=$(HOME="$tmp" WB_OS_RELEASE="$root/tests/fixtures/os-release-ubuntu" ./wb --profile-info)
printf '%s\n' "$out" | grep -q 'configured=centos'
printf '%s\n' "$out" | grep -q 'detected=generic'
printf '%s\n' "$out" | grep -q 'effective=centos'

# Auto is no longer a selectable/configurable profile.
if HOME="$tmp" ./wb --set-profile auto 2>/dev/null; then
    echo '--set-profile auto unexpectedly succeeded' >&2
    exit 1
fi
