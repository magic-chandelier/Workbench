#!/bin/sh
set -eu
out=$(./wb --self-test)
generic_actions=$(printf '%s\n' "$out" | sed -n 's/.*generic_actions=\([0-9][0-9]*\).*/\1/p')
centos_actions=$(printf '%s\n' "$out" | sed -n 's/.*centos_actions=\([0-9][0-9]*\).*/\1/p')
generic_checked=$(printf '%s\n' "$out" | sed -n 's/.*generic_syntax=\([0-9][0-9]*\).*/\1/p')
centos_checked=$(printf '%s\n' "$out" | sed -n 's/.*centos_syntax=\([0-9][0-9]*\).*/\1/p')
[ "$generic_checked" -eq "$generic_actions" ]
[ "$centos_checked" -eq "$centos_actions" ]
