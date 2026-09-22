#!/bin/sh
set -eu
make clean >/dev/null
make >/tmp/workbench-build.log 2>&1
if grep -E 'warning:' /tmp/workbench-build.log >/dev/null; then
  cat /tmp/workbench-build.log >&2
  echo 'build emitted warnings' >&2
  exit 1
fi
out=$(./wb --self-test 2>&1)
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -q '^SELFTEST PASS '
printf '%s\n' "$out" | grep -q 'profiles=2'
printf '%s\n' "$out" | grep -q 'generic_actions=374'
printf '%s\n' "$out" | grep -q 'centos_actions=483'
printf '%s\n' "$out" | grep -q 'centos_source=110'
printf '%s\n' "$out" | grep -q 'overrides=1'
printf '%s\n' "$out" | grep -q 'max_args=4'
printf '%s\n' "$out" | grep -q 'files_guard=PASS'
printf '%s\n' "$out" | grep -q 'terminal_guard=PASS'
printf '%s\n' "$out" | grep -q 'command_sets_guard=PASS'
