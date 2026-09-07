# Workbench Linux Core Module Design

## Goal

Refactor Workbench v0.7 so the TUI core no longer owns the Linux action database, and ship a substantially complete distro-independent Linux core module that can grow into the previously agreed 300–500+ Action range without editing the UI engine.

## Product boundary

The Linux core module targets **Linux Kernel + POSIX shell + common base user space**. It must not assume a specific distribution, package manager, init system, container stack, source-control system, or web server.

Excluded from linux-core: `apt`, `dnf`, `yum`, `pacman`, `apk`, `zypper`, `systemctl`, `journalctl`, Docker/Podman-specific workflows, Git-specific workflows, Nginx-specific workflows, and other module-worthy ecosystems.

A command may be present when it is common across Linux but not guaranteed on every minimal image. Such Actions must use `command -v` checks and provide a fallback when practical, otherwise a clear "not installed" result instead of failing mysteriously.

## Architecture

### Core

The core remains responsible for terminal handling, desktop/module navigation, category navigation, search, properties, argument prompting, risk confirmation, command execution, configuration, language switching, and mouse/keyboard interaction.

The core must not contain the linux-core Action catalogue.

### Built-in linux-core module

The shipped linux-core catalogue lives in dedicated source/data files under `modules/linux-core/`. At build time the catalogue is compiled into the `wb` executable so the default deployment remains a single binary. The runtime module interface is represented by a stable `Module`/`Task` model rather than by direct references to one global `TASKS[]` array.

This gives source-level modularity now while keeping v0.7's tiny deployment model. A later runtime loader can add external modules using the same model without another TUI redesign.

### Task model

Each Action retains:

- stable Task ID
- Chinese and English title
- Chinese and English description
- search keywords
- category
- risk class: normal / sensitive / privileged
- shell command template
- zero to four prompted parameters, each with Chinese/English prompt, default, and validation kind

The parameter capacity is raised from 2 to 4 because common workflows such as `find`, ownership/permission changes, archive creation, network probes, and process controls routinely need more than two independent values.

### Categories

The initial linux-core module keeps the existing navigation categories and expands them where useful without changing the three-level UX:

- System
- Files
- Text
- Process
- Network
- Storage
- Permission
- Archive
- User

The module's "All" view remains synthetic and is not a stored category.

## Action coverage

The goal is practical completeness, not the impossible claim that Linux has one standardized finite list of "all commands". Linux distributions expose different userlands and optional packages. The shipped module therefore targets **all broadly useful distro-independent operations that fit the agreed base-user-space boundary**, represented as task-oriented Actions rather than one row per executable.

Coverage should include, at minimum:

- system/kernel/host/time/environment/resource inspection
- file and directory listing, metadata, traversal, creation, copy, move, links, deletion, checksums, comparison
- text viewing, extraction, filtering, matching, sorting, counting, field processing, transforms, binary/hex inspection
- process discovery, tree/state/details, signalling, priority, waiting, scheduling hints, open descriptors where tools exist
- network interfaces, addresses, routes, neighbours, sockets, DNS, ICMP, HTTP, downloads, remote shell/copy, hostname resolution
- filesystem/disk usage, block devices, UUIDs, mounts, filesystem tables, sync, swap, I/O/proc statistics where tools exist
- permissions, ownership, modes, umask, ACL/xattr inspection where tools exist
- tar/gzip/bzip2/xz/zip-family operations with capability checks where optional tools are needed
- identity, groups, sessions, login history, password/account metadata, shell/home lookup

Target release size: at least **300 task-oriented Actions** while retaining quick search and category navigation.

## Safety

Normal actions are read-only or create harmless local output by default. Sensitive actions access a network or modify reversible user-level state. Privileged actions may delete data, alter ownership/permissions broadly, unmount filesystems, send destructive signals, or otherwise have meaningful impact.

Arguments must be shell-quoted before template substitution. Numeric/port/PID validations remain strict. Destructive actions must be narrow and explicit; no Action should encode catastrophic whole-system defaults such as `rm -rf /`, raw-disk overwrite, filesystem formatting, or firewall flushes in linux-core.

## Compatibility

- C11/POSIX-oriented implementation using the current libc-only runtime dependency target.
- No ncurses, Python, Node.js, JVM, or daemon requirement.
- `LANG=C/POSIX` must remain usable.
- Missing optional commands must fail clearly or fall back.
- `make clean && make` must compile with `-Wall -Wextra -Wpedantic` and zero warnings.

## Testing

Add non-interactive self-test support so catalogue and template integrity can be checked without driving the TUI manually. Tests must verify:

- module/action count and unique Task IDs
- valid categories/risks/argument schemas
- every `{N}` placeholder has a corresponding argument definition
- no undefined parameter placeholders
- search can match title/keywords/command/task ID
- dangerous banned patterns are absent from built-in templates
- the program builds with zero warnings
- `wb --self-test` exits 0

A small smoke test should also launch the TUI under a pseudo-terminal and verify the desktop renders.
