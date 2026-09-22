#!/bin/sh
set -eu
out=$(./wb --self-test)
printf '%s\n' "$out" | grep -q 'destructive_path_guard=PASS'
grep -q 'ARG_DESTRUCTIVE_PATH' workbench.h
grep -q 'files.remove_tree.*ARG_DESTRUCTIVE_PATH' modules/linux-core/actions/files.inc
grep -q 'perm.chmod_recursive.*ARG_DESTRUCTIVE_PATH' modules/linux-core/actions/permission.inc
