#!/bin/sh
set -eu
root=$(pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

HOME="$tmp" ./wb --set-profile centos
[ -f "$tmp/.config/workbench/config" ]
grep -q '^linux_profile=centos$' "$tmp/.config/workbench/config"
out=$(HOME="$tmp" WB_OS_RELEASE="$root/tests/fixtures/os-release-ubuntu" ./wb --profile-info)
printf '%s\n' "$out" | grep -q 'configured=centos'
printf '%s\n' "$out" | grep -q 'effective=centos'

HOME="$tmp" ./wb --set-profile generic
grep -q '^linux_profile=generic$' "$tmp/.config/workbench/config"
out=$(HOME="$tmp" WB_OS_RELEASE="$root/tests/fixtures/os-release-centos" ./wb --profile-info)
printf '%s\n' "$out" | grep -q 'configured=generic'
printf '%s\n' "$out" | grep -q 'detected=centos'
printf '%s\n' "$out" | grep -q 'effective=generic'
