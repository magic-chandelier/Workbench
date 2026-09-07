# Workbench Dual-Input UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every current Workbench workflow usable with keyboard-only or mouse-only input and add a first-party virtual keyboard for all text prompts.

**Architecture:** Keep the current raw terminal/SGR parser but introduce reusable semantic UI helpers for menus, buttons, confirmations, selectors and text entry. Migrate existing pages to these helpers incrementally, preserving all existing shortcut keys and filesystem/command safety behavior.

**Tech Stack:** C11/POSIX, libc, termios, poll, ANSI/SGR terminal sequences, Python PTY smoke tests for interaction verification.

**Spec:** `docs/superpowers/specs/2026-09-06-dual-input-ui-design.md`

## Global Constraints

- Preserve one `wb` binary and libc-only runtime dependencies.
- Keep Linux Generic at 374 Actions and CentOS effective catalogue at 483 Actions unless a separate catalogue change is explicitly requested.
- No ncurses/readline/GLib/Go/Rust/Python runtime dependency.
- Every current Workbench-owned interactive function must have both keyboard-only and mouse-only activation paths.
- Every text prompt must use the common input dialog and be operable by mouse through the virtual keyboard.
- Preserve Workbench Files POSIX-only mutation layer and all destructive path/symlink protections.
- Compile warning-free with `-Wall -Wextra -Wpedantic`.

---

### Task 1: Common UI event and hit-region primitives

**Files:**
- Modify: `main.c`
- Create: `tests/test_dual_input_structure.sh`
- Modify: `Makefile`

**Interfaces:**
- Produces semantic `UiEvent` data from the existing terminal parser.
- Produces a small hit-region API used by dialogs and pages.

- [ ] Write `tests/test_dual_input_structure.sh` requiring `UiEvent`, hit-region registration helpers, and v0.12 metadata.
- [ ] Run the test and confirm it fails against v0.11 code.
- [ ] Add the event/hit-region primitives without changing page behavior.
- [ ] Run the new test plus `tests/smoke_tui.py` and confirm both pass.

### Task 2: Unified confirmation and choice widgets

**Files:**
- Modify: `main.c`
- Create: `tests/smoke_dual_dialogs.py`

**Interfaces:**
- Produces reusable confirmation/choice helpers returning semantic accept/cancel/index results.
- Replaces `fgets()`-based `confirm_line()` and `confirm_command()`.

- [ ] Add PTY tests that require keyboard and SGR-mouse confirmation/cancellation paths.
- [ ] Run and observe expected failure on mouse confirmation.
- [ ] Implement reusable clickable confirmation and choice widgets in raw mode.
- [ ] Migrate generic and command confirmations.
- [ ] Run dialog tests and the existing destructive-path/profile tests.

### Task 3: Unified text input and virtual keyboard

**Files:**
- Modify: `main.c`
- Create: `tests/smoke_virtual_keyboard.py`

**Interfaces:**
- Produces `ui_prompt_text(...)` compatible with existing prompt validation call sites.
- Supports keyboard typing and mouse-click virtual keyboard entry into the same buffer.

- [ ] Add PTY tests for mouse-only ASCII/path entry, Backspace, Shift, Clear, Accept and Cancel.
- [ ] Run and observe failure because no virtual keyboard exists.
- [ ] Implement UTF-8-safe editable buffer and virtual keyboard rendering/hit testing.
- [ ] Replace `prompt_text()` internals with the common input component while retaining `validate_arg()`.
- [ ] Migrate Workbench Files create/copy/move/rename/chmod prompts to the common component.
- [ ] Run virtual-keyboard tests and existing file-manager operation/safety tests.

### Task 4: Settings and desktop dual-input migration

**Files:**
- Modify: `main.c`
- Create: `tests/smoke_mouse_only_settings.py`

**Interfaces:**
- Settings rows, profile options, confirmation-policy rows, autostart controls and Back expose mouse hit regions and existing keys.

- [ ] Add a mouse-only PTY flow for desktop -> Settings -> profile/language selection -> Back.
- [ ] Run and confirm the current settings UI fails the mouse-only flow.
- [ ] Add clickable settings rows, selectors and Back controls using the common widgets.
- [ ] Preserve arrows/k/j/Enter/Esc/q and existing direct shortcuts.
- [ ] Run the new settings test plus profile persistence tests.

### Task 5: Command workbench dual-input migration

**Files:**
- Modify: `main.c`
- Create: `tests/smoke_dual_commands.py`

**Interfaces:**
- Category/task/detail/search screens expose semantic mouse controls and keyboard equivalents.
- Search text uses the common input dialog.

- [ ] Add keyboard-only and mouse-only flows for category selection, task selection, properties, Back, Settings and search entry/results.
- [ ] Run and capture expected failures in properties/search mouse paths.
- [ ] Add clickable category/task/detail/search controls without changing catalogue data.
- [ ] Run new tests and existing catalogue/search/profile tests.

### Task 6: Workbench Files complete dual-input surface

**Files:**
- Modify: `modules/files-manager/files_ui.inc`
- Modify: `main.c` only for shared widget hooks where needed
- Modify: `tests/smoke_files.py`
- Create: `tests/smoke_mouse_only_files.py`

**Interfaces:**
- Existing Files mouse navigation is retained.
- Every file operation is available from visible mouse controls/context menu and from existing keyboard shortcuts.
- Viewer/properties/error screens expose clickable Back/navigation buttons.

- [ ] Add a mouse-only PTY workflow using a temporary HOME: Locations -> Home -> create -> rename -> properties -> delete -> desktop.
- [ ] Ensure the workflow uses no ordinary printable keyboard bytes for text; all text comes from virtual-keyboard clicks.
- [ ] Run and verify failure before migration.
- [ ] Add missing clickable controls to viewer/properties/dialog/action surfaces.
- [ ] Run new mouse-only test plus existing `smoke_files.py`, file-manager ops and safety tests.

### Task 7: Keyboard-only end-to-end regression

**Files:**
- Create: `tests/smoke_keyboard_only.py`
- Modify: `Makefile`

**Interfaces:**
- Provides a release gate proving no workflow requires mouse input.

- [ ] Add one PTY test that never sends SGR mouse sequences and covers desktop, Settings, commands/search, Files mutation and exit.
- [ ] Run it against the migrated UI and fix any keyboard regression without removing mouse paths.
- [ ] Add all new smoke/structure tests to `make test`.
- [ ] Run the complete suite.

### Task 8: Release metadata, self-test guard and packaging

**Files:**
- Modify: `main.c`
- Modify: `Makefile`
- Modify: `README.md`
- Modify: `CHANGELOG.md`
- Modify: `tests/test_release_metadata.sh`
- Modify: `tests/test_selftest.sh` if needed

**Interfaces:**
- Release version `0.12.0`.
- Documentation states the dual-input invariant and virtual-keyboard limits.

- [ ] Update metadata and user documentation.
- [ ] Add a self-test/structure guard ensuring no interactive production prompt uses `fgets(stdin)` for confirmations or task/file text entry.
- [ ] Run `make clean && make test` and inspect output for zero warnings/failures.
- [ ] Run `make package`.
- [ ] Extract `dist/workbench-v0.12.0.tar.gz` to a clean directory, rebuild and rerun the full test suite.
- [ ] Run `ldd wb` and verify no new runtime dependency beyond libc/system loader.
- [ ] Copy the verified source archive and x86_64 binary to `/mnt/data/` and compute SHA-256 hashes.
