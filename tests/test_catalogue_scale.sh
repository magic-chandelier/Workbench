#!/bin/sh
set -eu
out=$(./wb --self-test)
printf '%s\n' "$out"
get_count() {
  key=$1
  printf '%s\n' "$out" | sed -n "s/.*$key=\([0-9][0-9]*\).*/\1/p"
}
assert_min() {
  key=$1 min=$2
  got=$(get_count "$key")
  [ -n "$got" ] || { echo "missing count: $key" >&2; exit 1; }
  [ "$got" -ge "$min" ] || { echo "$key count $got < $min" >&2; exit 1; }
}
assert_min generic_actions 374
assert_min centos_actions 420
assert_min generic_system 35
assert_min generic_files 65
assert_min generic_text 45
assert_min generic_process 30
assert_min generic_network 40
assert_min generic_storage 30
assert_min generic_permission 18
assert_min generic_archive 20
assert_min generic_user 25
assert_min centos_packages 20
assert_min centos_services 15
assert_min centos_firewall 10
assert_min centos_selinux 10
