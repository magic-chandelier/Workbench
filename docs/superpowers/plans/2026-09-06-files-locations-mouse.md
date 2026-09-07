# Workbench Files Locations and Mouse Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a Linux-native Locations screen and complete mouse interaction to Workbench Files while preserving the libc-only single-binary architecture.

**Architecture:** Mount discovery and classification live in the existing files-manager C module; the TUI consumes a small location model. The existing SGR mouse parser in `main.c` remains the only input backend, while `files_ui.inc` adds location navigation, row hit-testing, double-click handling and a context menu.

**Tech Stack:** ISO C/POSIX APIs, Linux `/proc/self/mountinfo`, `statvfs(3)`, existing ANSI/SGR terminal code, Python PTY smoke tests.

**Spec:** `docs/superpowers/specs/2026-09-06-files-locations-mouse-design.md`

## Global Constraints

- No ncurses, GLib, Go, Rust, Python runtime, or other new runtime dependency.
- Do not shell out to `lsblk`, `mount`, `df`, `findmnt`, `cp`, `mv`, or `rm` for Workbench Files behavior.
- Existing Linux Generic and CentOS Action catalogues must not change.
- Existing destructive-path and symlink safety behavior must remain intact.
- Keyboard interaction remains available for every mouse-accessible action.

---

### Task 1: Locations data model and mountinfo parser

**Files:**
- Modify: `modules/files-manager/files_manager.h`
- Modify: `modules/files-manager/files_manager.c`
- Modify: `tests/test_files_manager_ops.c`

**Interfaces:**
- Produces `FmLocationKind`, `FmLocation`, `FmLocations`.
- Produces `fm_load_locations(const char *mountinfo_path, int show_system, FmLocations *out, char *err, size_t errn)` and `fm_free_locations(FmLocations *locations)`.

- [ ] Add unit assertions using a temporary mountinfo fixture containing root, local ext4/xfs, NFS, tmpfs and an escaped-space mount point.
- [ ] Run `tests/test_files_manager_ops.sh` and confirm it fails because the location API does not exist.
- [ ] Implement mountinfo token parsing, octal unescape, pseudo-filesystem filtering, mount classification, duplicate mount-point elimination and `statvfs()` metadata.
- [ ] Re-run `tests/test_files_manager_ops.sh` and confirm all filesystem and location tests pass.

### Task 2: Locations TUI and keyboard navigation

**Files:**
- Modify: `modules/files-manager/files_ui.inc`
- Modify: `tests/smoke_files.py`

**Interfaces:**
- Consumes `fm_load_locations()`.
- Produces a `Locations / 位置` screen opened by default and keyboard actions `Enter` open, `h` show/hide system mounts, `l` language, `q/Esc` return.
- Browser gains `g` for Locations and `~` for Home.

- [ ] Extend PTY smoke coverage to require the initial Locations screen and navigation to Home/System; run it and confirm failure against v0.10 behavior.
- [ ] Implement Locations rendering, Home/root rows, mount grouping/capacity text and keyboard navigation.
- [ ] Add browser `Locations`/`Home` navigation shortcuts and rendering.
- [ ] Re-run `tests/smoke_files.py` and confirm keyboard coverage passes.

### Task 3: Mouse hit-testing, double click and context menu

**Files:**
- Modify: `modules/files-manager/files_ui.inc`
- Modify: `tests/smoke_files.py`

**Interfaces:**
- Consumes existing global `mouse_x`, `mouse_y`, `mouse_button`, `mouse_release` and `KEY_MOUSE`.
- Produces browser row left-click selection, 500 ms same-row double-click open/view, clickable Locations/Home controls, location-row click-open and right-click context menu.

- [ ] Add PTY SGR mouse sequences for row clicks, button clicks, right click and wheel; run smoke test and confirm failure before UI code changes.
- [ ] Add stable row-coordinate bookkeeping to browser and Locations renderers.
- [ ] Implement monotonic-time double-click detection and browser mouse dispatch.
- [ ] Implement the right-click context menu by routing to the same POSIX operation helpers used by keyboard shortcuts.
- [ ] Re-run Files PTY smoke tests and confirm all keyboard and mouse paths pass.

### Task 4: Release metadata, self-test and packaging

**Files:**
- Modify: `main.c`
- Modify: `Makefile`
- Modify: `README.md`
- Modify: `CHANGELOG.md`
- Modify: `tests/test_release_metadata.sh`
- Modify: `tests/test_files_manager_safety.sh`

**Interfaces:**
- Release version becomes `0.11.0`.
- Existing `--self-test` additionally verifies location filtering/classification invariants without changing Action counts.

- [ ] Add release/safety assertions first and confirm they fail on v0.10 metadata.
- [ ] Update version metadata, documentation and self-test location guards.
- [ ] Run `make clean && make test` and require zero test failures and no compiler warnings.
- [ ] Run `make package`, extract the tarball into a fresh directory, rebuild and rerun `make test` there.
- [ ] Run `ldd wb` on the release binary and confirm no new library dependency beyond libc/loader.
