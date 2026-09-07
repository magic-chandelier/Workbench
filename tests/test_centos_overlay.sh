#!/bin/sh
set -eu

generic=$(./wb --catalogue-info generic)
printf '%s\n' "$generic" | grep -q 'profile=generic'
printf '%s\n' "$generic" | grep -q 'actions=374'
printf '%s\n' "$generic" | grep -q 'centos_source=0'
printf '%s\n' "$generic" | grep -q 'packages=0'
printf '%s\n' "$generic" | grep -q 'services=0'
printf '%s\n' "$generic" | grep -q 'firewall=0'
printf '%s\n' "$generic" | grep -q 'selinux=0'

centos=$(./wb --catalogue-info centos)
printf '%s\n' "$centos" | grep -q 'profile=centos'
actions=$(printf '%s\n' "$centos" | sed -n 's/.*actions=\([0-9][0-9]*\).*/\1/p')
centos_source=$(printf '%s\n' "$centos" | sed -n 's/.*centos_source=\([0-9][0-9]*\).*/\1/p')
[ "$actions" -gt 420 ]
[ "$centos_source" -gt 45 ]
printf '%s\n' "$centos" | grep -Eq 'packages=[1-9][0-9]*'
printf '%s\n' "$centos" | grep -Eq 'services=[1-9][0-9]*'
printf '%s\n' "$centos" | grep -Eq 'firewall=[1-9][0-9]*'
printf '%s\n' "$centos" | grep -Eq 'selinux=[1-9][0-9]*'
printf '%s\n' "$centos" | grep -Eq 'overrides=[1-9][0-9]*'

./wb --task-info generic centos.packages.search >/dev/null 2>&1 && exit 1
info=$(./wb --task-info centos centos.packages.search)
printf '%s\n' "$info" | grep -q 'source=centos'
printf '%s\n' "$info" | grep -q 'command=dnf search'

override=$(./wb --task-info centos system.hostname)
printf '%s\n' "$override" | grep -q 'source=centos'
printf '%s\n' "$override" | grep -q 'command=hostnamectl'
