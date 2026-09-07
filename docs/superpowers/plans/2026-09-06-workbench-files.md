# Workbench Files Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a first-party local file manager as the first Workbench desktop application while preserving the existing Linux/CentOS command workbench.

**Architecture:** Put filesystem enumeration and mutations in `modules/files-manager/files_manager.c`; keep terminal rendering/input in `main.c` so the new UI reuses Workbench's raw-terminal, bilingual, prompt, and confirmation mechanisms. No file operation is implemented through shell commands.

**Tech Stack:** C11-compatible C, POSIX/libc filesystem APIs, existing ANSI terminal UI.

**Spec:** `docs/superpowers/specs/2026-09-06-workbench-files-design.md`

## Global Constraints

- Workbench Files is desktop row 1; Linux/CentOS command workbench is below it.
- Single `wb` binary; no new runtime library dependency beyond libc/system loader.
- No third-party file-manager source.
- Filesystem mutations do not invoke `/bin/sh`.
- Linux Generic remains 374 Actions; CentOS effective catalogue remains 483 Actions.
- Recursive deletion must never accept `/`, `.`, `..`, or a path resolving to `/`.

---

### Task 1: Filesystem model and mutation primitives

**Files:**
- Create: `modules/files-manager/files_manager.h`
- Create: `modules/files-manager/files_manager.c`
- Create: `tests/test_files_manager_ops.c`
- Create: `tests/test_files_manager_ops.sh`
- Modify: `Makefile`

**Interfaces:**
- Produces `FmEntry`, `FmDirectory`, `fm_load_directory`, `fm_free_directory`, `fm_join_path`, `fm_parent_path`, `fm_create_file`, `fm_create_directory`, `fm_copy_path`, `fm_move_path`, `fm_remove_path`, `fm_chmod_path`, `fm_parse_mode`.

- [ ] Write `tests/test_files_manager_ops.c` to create a temporary tree and assert hidden filtering, create, copy, symlink preservation, move, chmod, recursive delete, and descendant-copy rejection.
- [ ] Add `tests/test_files_manager_ops.sh` to compile the test against `modules/files-manager/files_manager.c`; run it and verify RED because the module does not exist.
- [ ] Implement the minimal filesystem module using `opendir/readdir/lstat/open/read/write/mkdir/symlink/readlink/rename/unlink/rmdir/chmod`.
- [ ] Run `tests/test_files_manager_ops.sh` and verify GREEN.
- [ ] Add module source/header/test to `Makefile` and verify existing build stays warning-free.

### Task 2: Desktop placement and file-manager browser UI

**Files:**
- Modify: `main.c`
- Create: `tests/smoke_files.py`
- Modify: `tests/smoke_tui.py`

**Interfaces:**
- Consumes the filesystem module from Task 1.
- Produces `files_manager_menu(AppConfig *)` and desktop routing with Workbench Files at index 0.

- [ ] Write a PTY smoke test that expects `Workbench Files` above `Linux Generic Command Set`, presses Enter on the first row, sees the file-manager path screen, exits to desktop, then exits Workbench; verify RED.
- [ ] Add browser rendering with current path, rows, size/type/mode, hidden toggle, parent navigation, and selection movement.
- [ ] Update desktop row routing and mouse row routing so index 0 opens Workbench Files and index 1 opens the command workbench.
- [ ] Run the PTY file-manager smoke test and existing TUI profile-switch smoke test; verify GREEN.

### Task 3: Viewer, properties, and safe mutations in TUI

**Files:**
- Modify: `main.c`
- Modify: `tests/smoke_files.py`

**Interfaces:**
- Adds file-manager actions `v/p/n/N/c/m/r/d/x/h` to the browser.

- [ ] Extend PTY smoke fixture with visible/hidden files and assert `h` reveals hidden entries and `p` shows properties; verify RED.
- [ ] Implement properties screen using `lstat`, user/group lookup when available, symlink target display, timestamps, and mode.
- [ ] Implement bounded read-only text viewer that rejects NUL-containing binary input.
- [ ] Implement create/copy/move/rename/delete/chmod prompts using direct filesystem module calls; destructive delete must use explicit confirmation.
- [ ] Re-run file-manager operation unit test and PTY smoke; verify GREEN.

### Task 4: Self-test, release metadata, docs, and packaging

**Files:**
- Modify: `main.c`
- Modify: `Makefile`
- Modify: `README.md`
- Modify: `CHANGELOG.md`
- Modify: `tests/test_release_metadata.sh`

**Interfaces:**
- Release version `0.10.0`.

- [ ] Add a small file-manager primitive check to `--self-test` without mutating user data (mode parser/path safety only).
- [ ] Bump `WB_VERSION` and `VERSION` to `0.10.0`; update release metadata test and run it RED then GREEN.
- [ ] Document Workbench Files controls, safety boundary, and architecture; add changelog entry.
- [ ] Run `make clean && make test` and verify every test passes with no compiler warnings.
- [ ] Run `make package`, extract the tarball into a fresh directory, run `make clean && make test`, `./wb --self-test`, and PTY smoke tests again.
- [ ] Verify `ldd wb` has no new third-party runtime library and calculate SHA-256 for source archive and binary.
