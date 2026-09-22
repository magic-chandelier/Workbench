#!/bin/sh
set -eu
out=$(./wb --self-test)
printf '%s\n' "$out" | grep -q 'plugins_guard=PASS'
./wb --version | grep -q '^Workbench 0.16.1-dev4$'
./wb --version | grep -q '^Plugin System 0.2.0 (API 3, compatible 1-3)$'
