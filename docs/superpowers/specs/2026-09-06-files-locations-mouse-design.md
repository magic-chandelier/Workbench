# Workbench Files Locations and Mouse Design

## Goal

Extend Workbench Files with a first-class `Locations / 位置` screen for multi-disk Linux systems and complete mouse navigation without adding ncurses or any other runtime dependency.

## Product behavior

- Opening `Workbench Files` starts on `Locations / 位置`, not an arbitrary current working directory.
- Locations always shows the user's Home directory as a convenience entry and the Linux root filesystem `/` as `System`.
- Mounted user storage is shown as Linux mount points, not Windows drive letters.
- Each mount shows mount point, source, filesystem type, total/used capacity, and read-only/read-write state when available.
- Local mounts and network mounts are grouped visually. Network filesystems include NFS, CIFS/SMB, SSHFS, 9p, Ceph and GlusterFS families.
- Pseudo/system mounts such as proc, sysfs, devtmpfs, devpts, tmpfs, cgroup/cgroup2, securityfs, debugfs, tracefs and similar internal mounts are hidden by default.
- `Show system mounts / 显示系统挂载` toggles the filtered mounts for advanced users.
- The ordinary browser has clickable `Locations` and `Home` navigation controls.
- Keyboard use remains fully supported.

## Linux implementation

- Parse `/proc/self/mountinfo` directly. Do not shell out to `lsblk`, `mount`, `df`, or `findmnt`.
- Support mountinfo escaping (`\\040`, `\\011`, `\\012`, `\\134`).
- Use `statvfs()` on the mount point to obtain capacity and availability.
- Root `/` is always visible even when its filesystem type would otherwise be filtered (for example `overlay` in containers).
- A test-only `WB_MOUNTINFO` environment override may point production code at a fixture, matching the existing `WB_OS_RELEASE` testing style.
- Failed `statvfs()` calls do not discard the mount; capacity fields are marked unavailable.
- Duplicate mount-point rows are collapsed so the screen is not noisy.

## Mouse interaction

Workbench already enables SGR mouse reporting (1000/1006) in its terminal input layer. Workbench Files will consume those events.

- Wheel up/down continues to map to selection movement.
- Left click on a visible browser row selects it.
- A second left click on the same row within 500 ms opens a directory or views a regular file (double click).
- Left click on `Locations` opens the Locations screen; left click on `Home` opens `$HOME`.
- Locations rows are opened by left click; a double-click is not required because a location row itself is a navigation target.
- Right click on a normal file browser row selects it and opens a small context menu containing Open/View, Properties, Copy, Move, Rename, Delete, chmod and Back. The parent (`..`) row only exposes Open/Back.
- Mouse support must never remove keyboard shortcuts.

## Safety

- All filesystem mutations continue to use the existing POSIX-based files-manager operation layer and never interpolate paths into a shell command.
- Existing root-path, recursive-copy, symlink and control-character protections remain unchanged.
- A mount point is merely a navigation target. Locations never mounts, unmounts, formats, repairs, or changes a filesystem.
- Read-only mount state is informative; operations still rely on kernel permissions and existing error handling.
- Location strings from mountinfo are sanitized before terminal rendering.

## Architecture

`modules/files-manager/files_manager.[ch]` owns mount discovery, mountinfo parsing, filtering/classification and capacity metadata. `modules/files-manager/files_ui.inc` owns the Locations TUI, browser navigation buttons, double-click tracking and context menu. `main.c` keeps the existing SGR mouse parser; no new terminal library is introduced.

## Tests

- C unit tests use a synthetic mountinfo fixture plus temporary directories to verify parsing, escaping, classification, default filtering, system-mount visibility, root retention and capacity metadata.
- PTY smoke tests verify Files opens on Locations, keyboard navigation into Home/System, mouse click on a location, browser row click/double-click, mouse navigation buttons, wheel behavior and right-click context menu.
- Existing file operation, profile, catalogue and desktop tests must continue to pass unchanged.
