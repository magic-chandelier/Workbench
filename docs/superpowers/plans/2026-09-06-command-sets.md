# Command Sets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a first-class Command Sets container with a read-only Linux system set, editable persisted user sets/actions, and one shared browser used by desktop execute mode and Terminal insert mode.

**Architecture:** Add a focused `modules/command-sets` subsystem for model/storage/mutation APIs. Keep built-in Linux tasks in the existing effective catalogue, expose them as a read-only system set, and store only user-created sets as `.wbc` files. Refactor the current command browser so the caller selects EXECUTE or INSERT behavior instead of maintaining a separate Terminal palette.

**Tech Stack:** C11/POSIX C, libc, current Workbench `Task`/`UiEvent` infrastructure, PTY tests in Python, shell/C unit tests.

**Spec:** `docs/superpowers/specs/2026-09-06-command-sets-design.md`

## Global Constraints

- Keep the runtime single-binary and libc-only; do not add JSON/YAML/SQLite or third-party libraries.
- Linux system set is read-only at the mutation API layer.
- User storage is `~/.local/share/workbench/command-sets/`, directory mode `0700`, set files mode `0600`.
- Maximum 64 user command sets and 256 actions per user set.
- Desktop selection executes through existing Workbench safety/confirmation; Terminal selection only inserts rendered text and never sends Enter.
- Existing Generic/CentOS catalogue sizes and overlay semantics remain unchanged.
- Keyboard-only and mouse-only paths must both remain supported.

---

### Task 1: Command Set Core Model and System Read-only Guard

**Files:**
- Create: `modules/command-sets/command_sets.h`
- Create: `modules/command-sets/command_sets.c`
- Create: `tests/test_command_sets_core.c`
- Create: `tests/test_command_sets_core.sh`
- Modify: `Makefile`

**Interfaces:**
- Produces `CommandSetSource`, `CommandSet`, `CommandSetStore`.
- Produces `wb_command_sets_init(...)`, `wb_command_set_system_linux(...)`, `wb_command_set_can_edit(...)`, `wb_command_set_rename(...)`, `wb_command_set_delete(...)`.

- [ ] Write a C test that initializes the Linux set against a sample/effective `Task` array and asserts source=SYSTEM, read_only=1, task pointer/count passthrough, rename failure, delete failure.
- [ ] Run `tests/test_command_sets_core.sh` and verify it fails because the subsystem is missing.
- [ ] Implement the smallest model/read-only API needed to pass.
- [ ] Add the new C source/header to the main Makefile build dependencies.
- [ ] Run the focused test and `make clean && make`; require zero warnings.

### Task 2: User `.wbc` Storage

**Files:**
- Modify: `modules/command-sets/command_sets.h`
- Modify: `modules/command-sets/command_sets.c`
- Create: `tests/test_command_sets_storage.c`
- Create: `tests/test_command_sets_storage.sh`

**Interfaces:**
- Produces `wb_command_sets_load_user(...)`, `wb_command_set_create_user(...)`, `wb_command_set_save(...)`, `wb_command_set_delete_user(...)`, `wb_command_sets_free(...)`.
- Storage uses a caller-supplied home/data root in tests and the default XDG/home path in production.

- [ ] Write a C test using `mkdtemp` that creates/renames/reloads/deletes a user set; assert directory `0700` and `.wbc` `0600`.
- [ ] Add cases for escaping round-trip, malformed file skip, duplicate ID rejection, system-ID rejection, and the 64-set limit.
- [ ] Run the focused test and verify red.
- [ ] Implement line-oriented `.wbc` parsing/serialization and atomic save.
- [ ] Run the storage test and warning-free build.

### Task 3: User Action CRUD and Parameter Round-trip

**Files:**
- Modify: `modules/command-sets/command_sets.h`
- Modify: `modules/command-sets/command_sets.c`
- Create: `tests/test_command_sets_actions.c`
- Create: `tests/test_command_sets_actions.sh`

**Interfaces:**
- Produces `wb_command_action_add(...)`, `wb_command_action_update(...)`, `wb_command_action_delete(...)`, `wb_command_action_find(...)`.
- Reuses `Task`, `Risk`, `ArgKind`, and `TaskArg` from `workbench.h`.

- [ ] Write tests for add/update/delete/reload of a no-argument action and a `{1}`/`{2}` action including prompts/default/kinds/risk/keywords.
- [ ] Assert system-set action mutation is rejected and user-set limit 256 is enforced.
- [ ] Run focused test and verify red.
- [ ] Implement minimal action ownership, serialization, mutation, and validation.
- [ ] Run all three Command Set C test scripts and warning-free main build.

### Task 4: Command Sets Container UI and Desktop Execute Mode

**Files:**
- Modify: `main.c`
- Create: `tests/smoke_command_sets_hierarchy.py`
- Create: `tests/smoke_command_sets_crud.py`

**Interfaces:**
- Produces `CommandBrowserMode { COMMAND_MODE_EXECUTE, COMMAND_MODE_INSERT }` and `command_sets_menu(AppConfig *, CommandBrowserMode, char *, size_t)`.
- Linux set delegates to existing category/task menus.
- User set uses dedicated custom-action browser/editor.

- [ ] Write PTY hierarchy test: desktop third item is Command Sets; entering it shows Linux Commands `[System] [Read-only]`; Enter then reaches categories.
- [ ] Run and verify it fails on v0.14 direct-Linux navigation.
- [ ] Refactor desktop to call the Command Sets container.
- [ ] Write keyboard CRUD PTY test creating a set and action, executing it, editing/deleting action, renaming/deleting set, and verifying filesystem persistence.
- [ ] Run and verify red, then implement user set/action editors reusing `ui_input_dialog` and existing execution safety.
- [ ] Run both PTY tests plus focused C tests.

### Task 5: Mouse-only CRUD and Shared Terminal Insert Mode

**Files:**
- Modify: `main.c`
- Create: `tests/smoke_command_sets_mouse.py`
- Create: `tests/smoke_terminal_command_sets.py`

**Interfaces:**
- Terminal control action invokes `command_sets_menu(..., COMMAND_MODE_INSERT, ...)`.
- INSERT returns rendered command bytes without writing newline to PTY.

- [ ] Write mouse-only CRUD PTY test that enables virtual keyboard, creates/edits/deletes a user set/action using only SGR mouse events.
- [ ] Run and verify red, then add hit regions/buttons for every user-set mutation path.
- [ ] Write Terminal PTY test that navigates the same Command Sets hierarchy to Linux and user actions; verify selection inserts but does not execute until Enter.
- [ ] Remove the old flattened Terminal command palette and route Terminal control to the shared browser.
- [ ] Run mouse-only and Terminal shared-browser tests plus existing Terminal tests.

### Task 6: Self-test, Release Metadata, Docs, and Full Regression

**Files:**
- Modify: `main.c`
- Modify: `Makefile`
- Modify: `README.md`
- Modify: `CHANGELOG.md`
- Modify: `tests/test_selftest.sh`
- Modify: `tests/test_release_metadata.sh`
- Modify: `tests/test_source_package.sh` if needed for new source files

**Interfaces:**
- `wb --self-test` adds `command_sets_guard=PASS` without changing existing catalogue statistics.
- Release version becomes `0.15.0`.

- [ ] Add self-test guard for system-set read-only invariants and user-storage parser sanity.
- [ ] Add all new test scripts to `make test`.
- [ ] Update README with Command Sets hierarchy, read-only system set, user storage/limits, and Terminal INSERT semantics.
- [ ] Add v0.15.0 CHANGELOG entry and bump `WB_VERSION`/Makefile version.
- [ ] Run `make clean && make` and verify no compiler warning.
- [ ] Run `make test` and require all old/new tests green.
- [ ] Run `make package-only`; inspect archive to verify source-only contents and new command-set module/tests/docs included.
- [ ] Extract the generated source tar to a fresh directory, run `make clean && make test`, `./wb --self-test`, `ldd wb`, and `file wb`.
- [ ] Copy final source tar and x86_64 binary to `/mnt/data`, calculate SHA-256, and report exact verification results.
