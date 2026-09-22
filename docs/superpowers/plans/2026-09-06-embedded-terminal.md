# Embedded Terminal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an in-Workbench PTY shell with bounded in-memory history, mouse/keyboard controls, command insertion, and configurable history behavior.

**Architecture:** A new `modules/terminal/` subsystem owns PTY lifecycle and bounded terminal text state. `main.c` owns Workbench UI integration and settings, polls the PTY while Terminal is open, and uses existing Workbench input and task rendering for terminal controls and task insertion.

**Tech Stack:** C11/POSIX, Linux PTY ioctls, existing raw SGR mouse layer; no new runtime libraries.

**Spec:** `docs/superpowers/specs/2026-09-06-embedded-terminal-design.md`

## Global Constraints
- Keep Workbench libc/POSIX-only with no vendored terminal emulator.
- Default terminal history limit is exactly 1000 logical lines.
- Default history navigation chunk is exactly 50 lines.
- Clear-on-open defaults to Disabled.
- Terminal history is memory-only.
- Leaving Terminal does not terminate a live shell.
- Existing generic/CentOS action counts and Files behavior must not change.

---

### Task 1: Terminal session core
**Files:** Create `modules/terminal/terminal.h`, `modules/terminal/terminal.c`; create `tests/test_terminal_core.c`, `tests/test_terminal_core.sh`; modify `Makefile`.

**Interfaces:** Produce `WbTerminalSession`, `wb_terminal_init`, `wb_terminal_start`, `wb_terminal_poll`, `wb_terminal_send`, `wb_terminal_end`, `wb_terminal_clear`, `wb_terminal_set_limit`, `wb_terminal_line_count`, `wb_terminal_line_at`, and `wb_terminal_is_running`.

- [ ] Write core tests for 1000-line bounded history, CR/LF/backspace parsing, ANSI stripping, clear behavior, and a PTY `/bin/sh` echo round trip.
- [ ] Run the core test and verify RED because the terminal module does not exist.
- [ ] Implement the minimum PTY/session/parser required by those tests.
- [ ] Run core tests and existing `make test` subsets until GREEN.

### Task 2: Config and numeric settings
**Files:** Modify `main.c`; create `tests/test_terminal_config.sh`; extend `tests/smoke_ui_preferences.py`.

**Interfaces:** Persist `terminal_log_lines`, `terminal_load_lines`, and `terminal_clear_on_open` in `AppConfig`.

- [ ] Write tests for defaults 1000/50/0 and persistence.
- [ ] Verify RED.
- [ ] Add dual-input numeric picker controls with fixed +/- steps and bounds 100..10000 / 10..500.
- [ ] Verify settings tests and old config compatibility GREEN.

### Task 3: Desktop and embedded Terminal UI
**Files:** Modify `main.c`; create `tests/smoke_terminal.py`, `tests/smoke_terminal_mouse.py`, `tests/smoke_terminal_keyboard.py`.

**Interfaces:** Produce `terminal_menu(AppConfig*)` and a process-lifetime terminal session.

- [ ] Add failing PTY smoke asserting desktop order and Terminal opens without leaving Workbench UI.
- [ ] Implement desktop row and terminal UI with live PTY polling, toolbar, scroll offset, reopen preservation, clear-on-open, manual clear, end/restart, and Back.
- [ ] Add keyboard-only `Ctrl+]` control menu and mouse toolbar tests.
- [ ] Verify shell continues across Back/reopen and history remains available.

### Task 4: Command palette insertion
**Files:** Modify `main.c`; extend `tests/smoke_terminal.py`.

**Interfaces:** Add a Terminal task picker that calls existing `build_task_command` and sends the rendered command to the PTY without newline.

- [ ] Write failing test selecting a safe Linux Core task and verifying it appears at the shell prompt without running.
- [ ] Implement picker/search and command insertion.
- [ ] Verify user can edit/run via keyboard and mouse Run action.

### Task 5: Release, self-test, and source package
**Files:** Modify `main.c`, `Makefile`, `README.md`, `CHANGELOG.md`, release tests.

**Interfaces:** Release as v0.14.0 and keep source-only packaging.

- [ ] Add terminal guard and metadata tests.
- [ ] Update docs with Terminal scope and explicit limited xterm compatibility statement.
- [ ] Run `make clean && make test`, inspect compiler warnings, run `make package-only`.
- [ ] Extract the release tar, rebuild, rerun all tests, inspect `ldd`, architecture, tar contents, and SHA-256.
