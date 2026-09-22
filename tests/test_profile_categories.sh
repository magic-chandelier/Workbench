#!/bin/sh
set -eu

g=$(./wb --visible-categories generic)
printf '%s\n' "$g" | grep -q 'profile=generic'
printf '%s\n' "$g" | grep -q 'System'
printf '%s\n' "$g" | grep -q 'Files'
! printf '%s\n' "$g" | grep -q 'Packages'
! printf '%s\n' "$g" | grep -q 'Services'
! printf '%s\n' "$g" | grep -q 'Firewall'
! printf '%s\n' "$g" | grep -q 'SELinux'

c=$(./wb --visible-categories centos)
printf '%s\n' "$c" | grep -q 'profile=centos'
printf '%s\n' "$c" | grep -q 'Packages'
printf '%s\n' "$c" | grep -q 'Services'
printf '%s\n' "$c" | grep -q 'Firewall'
printf '%s\n' "$c" | grep -q 'SELinux'

info=$(./wb --task-info centos centos.firewall.state)
printf '%s\n' "$info" | grep -q 'source=centos'
