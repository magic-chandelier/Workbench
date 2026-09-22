#!/bin/sh
set -eu
for f in system files text process network storage permission archive user; do
  [ -f "modules/linux-core/actions/$f.inc" ]
  grep -q "actions/$f.inc" modules/linux-core/linux_core.c
done
for f in packages services firewall selinux network system; do
  [ -f "modules/centos/actions/$f.inc" ]
  grep -q "actions/$f.inc" modules/centos/centos.c
done
