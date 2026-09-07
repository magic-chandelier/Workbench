# Workbench GitHub Release & Licensing Design

## Goal

Prepare Workbench for public GitHub distribution while preserving maximum control over Workbench's own licensing and keeping third-party/upstream license boundaries explicit.

## Release model

- The GitHub repository contains Workbench source, tests, docs, and project policy files.
- `workbench-vX.Y.Z-source.tar.gz` is a source-only release archive and must not contain a compiled `wb` executable.
- Architecture-specific binaries such as `wb-vX.Y.Z-x86_64` are separate Release assets.

## Workbench license

Workbench uses a project-specific source-available license named **Workbench Source License 1.0**. The grant is intentionally narrower than OSI open-source licenses: source inspection, learning, personal non-commercial/internal evaluation, and private modification are permitted; redistribution, resale, hosted-service use, commercial product integration, sublicensing, or public derivative distribution require separate written permission.

The license grants no trademark rights and does not attempt to relicense third-party software invoked by Workbench. A contribution grant clause preserves the project's ability to use, modify, distribute, sublicense, and relicense submitted contributions.

## Third-party boundary

Workbench must distinguish three relationships:

1. **Workbench-owned implementation**: TUI, modules, Files, profile/overlay logic, action catalogue, safety and input system.
2. **Platform interfaces/runtime**: Linux system calls/UAPI, POSIX interfaces, host C library. Linux userspace syscall use does not copy kernel code into Workbench; release binaries dynamically use the host C runtime.
3. **External programs**: shell commands such as `tar`, `gzip`, `ip`, `systemctl`, `dnf`, `rpm`, `ssh`, and others are invoked as separate host-installed processes. Their source/binaries are not bundled in Workbench source or binary releases.

`THIRD_PARTY_NOTICES.md` and README must state this distinction plainly.

## Dependency admission policy

- Prefer Workbench-owned code, POSIX/Linux APIs, and execution of already-installed system programs.
- Do not vendor third-party source or binaries by default.
- Permissive-license source and LGPL libraries require explicit review before inclusion/linking.
- GPL/AGPL source or libraries must not enter Workbench core without an explicit architecture/license decision.
- Do not copy third-party manuals/documentation as Workbench documentation.
- Every new bundled/link-time dependency requires a documented license review and notice update.

## CentOS trademark handling

CentOS is used only as a compatibility/profile descriptor. README and notices state that CentOS is a trademark of Red Hat, Inc. and that Workbench is not affiliated with or endorsed by Red Hat or the CentOS Project. Workbench must not use CentOS logos or name its own product/service as a CentOS-branded product.

## README component inventory

README will include a component/relationship table covering:

- Linux kernel interfaces
- host C library (glibc on the distributed x86_64 build)
- POSIX `/bin/sh`
- GNU/base user-space commands where present
- util-linux/procps/findutils/grep/sed/awk-family tools as host-provided utilities
- iproute2 and optional legacy networking tools
- curl/wget, OpenSSH, BIND DNS utilities
- tar/gzip/bzip2/xz/zip/unzip/cpio/ar
- CentOS profile tools: DNF/RPM, systemd, firewalld, SELinux utilities, NetworkManager/nmcli

The table must say that exact implementations vary by distribution and these programs are not bundled.

## Verification

Release tests must verify:

- version metadata is v0.13.0;
- LICENSE, THIRD_PARTY_NOTICES.md, DEPENDENCY_POLICY.md are present;
- README contains the component inventory and CentOS trademark disclaimer;
- source package contains required project files but no top-level `wb` executable;
- source package can be extracted, built, and pass the existing test suite;
- binary still links only to the system C runtime/loader in the current build environment.
