# Third-Party Notices

Workbench is an independently implemented terminal application. The Workbench
source tree and official `wb` binary do **not bundle** the source code or
binaries of the external command-line programs listed below. Many Workbench
Actions simply start programs already installed by the host Linux system.

This file documents the boundary; it does not replace the license notices of
those third-party projects. Exact implementations and licenses can vary by
Linux distribution and package version.

Workbench Terminal itself is implemented in the Workbench source tree using Linux/POSIX pseudo-terminal interfaces. It does **not** embed a third-party terminal emulator or shell implementation.

## Platform/runtime relationship

| Component | Relationship to Workbench | Typical license context |
|---|---|---|
| Linux kernel interfaces | Workbench uses normal system calls and Linux userspace interfaces such as `/proc` and `/proc/self/mountinfo`; Linux kernel source is not bundled. | Linux kernel is GPL-2.0-only with the Linux syscall exception for normal userspace API use. |
| Host C library | Workbench uses standard C/POSIX interfaces. The distributed x86_64 build is dynamically linked to the host glibc runtime in the release build environment; glibc itself is not copied into the source archive. | GNU C Library is LGPL-2.1-or-later. |
| POSIX shell | Some Action templates execute through `/bin/sh`; the shell implementation comes from the host. | License depends on the host shell implementation. |
| Interactive shell used by Workbench Terminal | Workbench creates a PTY and starts the executable referenced by `$SHELL`, falling back to `/bin/sh`. The shell remains a separate host-provided child process and is not bundled or linked into `wb`. | License depends on the selected host shell (for example Bash, Zsh, Fish, Dash, BusyBox `sh`, etc.). |

## External host programs used by Actions

The following are representative external projects/tools that Workbench may
invoke when installed on the host. They are not linked into Workbench and are
not bundled by the Workbench release.

| Project / family | Representative commands | Typical upstream license family |
|---|---|---|
| GNU/base user-space utilities and distribution alternatives | `ls`, `cp`, `mv`, `rm`, `cat`, `sort`, `head`, `tail`, `chmod`, `chown`, `grep`, `sed`, `find`, `awk`, and related tools | Varies by implementation; GNU packages are commonly GPL-family, while BusyBox/other implementations differ. |
| util-linux / procps-ng and similar host utilities | `mount`, `umount`, `lsblk`, `blkid`, `swapon`, `ps`, `top`, `free`, `uptime`, `kill`, and related tools | Project-specific open-source licenses; host package decides the exact implementation. |
| iproute2 | `ip`, `ss` | GPL-2.0 / GPL-2.0-or-later across project files. |
| Legacy/network diagnostic implementations | `ifconfig`, `netstat`, `arp`, `route`, `ping`, `traceroute` | Depends on the host package/project. |
| curl / wget | `curl`, `wget` | curl uses the curl license; GNU Wget is GPL-family. |
| OpenSSH | `ssh`, `scp` | OpenSSH uses BSD-style and other permissive notices documented by the project. |
| BIND DNS utilities | `dig`, `host`, `nslookup` | Current BIND 9 is MPL-2.0. |
| GNU tar / gzip / cpio and host alternatives | `tar`, `gzip`, `gunzip`, `cpio` | GNU implementations are GPL-family; exact implementation may differ by system. |
| bzip2 | `bzip2`, `bunzip2` | BSD-style bzip2 license. |
| XZ Utils | `xz`, `unxz` | Current XZ Utils contains 0BSD and other project-specific license notices depending on file/version. |
| Info-ZIP or distribution alternatives | `zip`, `unzip` | Project-specific permissive/Info-ZIP license when provided by Info-ZIP; distribution implementation may differ. |
| binutils or distribution alternatives | `ar` | GNU binutils is GPL-family; implementation may differ. |

## CentOS compatibility profile

When the user manually enables the CentOS compatibility profile, Workbench can
invoke additional host-installed tools. These programs remain separate from
Workbench:

| Project / subsystem | Representative commands | Typical upstream license context |
|---|---|---|
| DNF / DNF5 | `dnf` / `dnf5` | DNF5 code includes GPL-2.0-or-later and LGPL-2.1-or-later licensing choices for libraries/components. |
| RPM | `rpm` | RPM code is GPL-2.0-or-later, with LGPL alternatives for specified library portions. |
| systemd | `systemctl`, `journalctl`, `hostnamectl` | systemd is primarily LGPL-2.1-or-later, with documented exceptions/components under other licenses. |
| firewalld | `firewall-cmd` | GPL-family project licensing. |
| SELinux user-space tools | `getenforce`, `setenforce`, `restorecon`, `semanage` | License depends on the specific SELinux userspace component/package. |
| NetworkManager | `nmcli` | NetworkManager project licensing applies to the host-installed program. |

## Trademark notice

CentOS is a trademark of Red Hat, Inc. Workbench uses the CentOS name only to
describe a compatibility/profile target. Workbench is not affiliated with or
endorsed by Red Hat or the CentOS Project. No CentOS logo is distributed as
part of Workbench.

All other product names, project names, and trademarks are the property of
their respective owners.
