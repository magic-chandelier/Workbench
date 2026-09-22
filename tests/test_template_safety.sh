#!/bin/sh
set -eu
for src in modules/linux-core/actions modules/centos/actions; do
  ! grep -R -n --include='*.inc' 'eval ' "$src"
  ! grep -R -n --include='*.inc' "sh -c 'exec 3<>/dev/tcp" "$src"
  ! grep -R -n --include='*.inc' -E 'mkfs\.|dd .*of=/dev/|nft flush ruleset|iptables -F' "$src"
done
! grep -R -n --include='*.inc' -E 'cd \{2\} && (cpio|ar) ' modules/linux-core/actions
