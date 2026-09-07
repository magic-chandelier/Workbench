#!/bin/sh
set -eu

[ -f LICENSE ]
[ -f THIRD_PARTY_NOTICES.md ]
[ -f DEPENDENCY_POLICY.md ]

grep -q '^Workbench Source License 1.0$' LICENSE
grep -q 'Third-Party Software' LICENSE
grep -q 'Commercial Use' LICENSE
grep -q 'Redistribution' LICENSE

grep -q '^# Third-Party Notices' THIRD_PARTY_NOTICES.md
grep -q 'not bundled' THIRD_PARTY_NOTICES.md
grep -q 'glibc' THIRD_PARTY_NOTICES.md
grep -q 'systemd' THIRD_PARTY_NOTICES.md
grep -q 'DNF' THIRD_PARTY_NOTICES.md
grep -q 'CentOS is a trademark of Red Hat, Inc.' THIRD_PARTY_NOTICES.md

grep -q '^# Dependency Admission Policy' DEPENDENCY_POLICY.md
grep -q 'GPL' DEPENDENCY_POLICY.md
grep -q 'AGPL' DEPENDENCY_POLICY.md
grep -q 'third-party binaries' DEPENDENCY_POLICY.md

grep -q '^# Workbench v0.15.0$' README.md
grep -q '^## 组件、外部工具与许可证边界' README.md
grep -q 'Linux kernel' README.md
grep -q 'glibc' README.md
grep -q 'iproute2' README.md
grep -q 'systemd' README.md
grep -q 'DNF / RPM' README.md
grep -q 'OpenSSH' README.md
grep -q 'BIND' README.md
grep -q 'tar / gzip / bzip2 / xz / zip / unzip / cpio / ar' README.md
grep -q '不随 Workbench 分发' README.md
grep -q 'CentOS is a trademark of Red Hat, Inc.' README.md
grep -q 'not affiliated with or endorsed by Red Hat or the CentOS Project' README.md

grep -q '^## 0.15.0' CHANGELOG.md
