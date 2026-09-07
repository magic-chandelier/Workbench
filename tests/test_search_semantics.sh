#!/bin/sh
set -eu
out=$(./wb --self-test)
printf '%s\n' "$out" | grep -q 'search_checked=5'
