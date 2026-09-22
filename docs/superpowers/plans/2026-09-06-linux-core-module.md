# Linux Core Module Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor Workbench v0.7 into a module-backed core and ship at least 300 distro-independent Linux task Actions.

**Architecture:** Move the Linux action catalogue out of `main.c` into a dedicated built-in `modules/linux-core` source unit while keeping it compiled into the single `wb` binary. Generalize core navigation/search/execution around a `Module` object, raise Action parameter capacity to four, and add a non-interactive catalogue self-test before expanding the module.

**Tech Stack:** C11/POSIX C, POSIX shell command templates, Make, libc-only runtime

**Spec:** `docs/superpowers/specs/2026-09-06-linux-core-module-design.md`

## Global Constraints

- Linux core boundary: Linux Kernel + POSIX shell + common base user space.
- Exclude distribution/package-manager/init-system-specific workflows.
- Default deployment remains a single binary.
- No ncurses, Python, Node.js, JVM, or daemon dependency.
- Preserve Simplified Chinese / English UI and `LANG=C/POSIX` rendering.
- Preserve three-level desktop → module → category → Action navigation semantics.
- Compile with `-Wall -Wextra -Wpedantic` and zero warnings.
- Ship at least 300 task-oriented linux-core Actions.

---

### Task 1: Catalogue integrity test hook

**Files:**
- Modify: `main.c`
- Create: `tests/test_selftest.sh`

**Interfaces:**
- Produces: `wb --self-test`, exit 0 only when catalogue invariants pass.

- [ ] Add a shell test that builds `wb`, invokes `./wb --self-test`, and asserts success plus an `actions=` summary.
- [ ] Run the test and verify it fails because `--self-test` is not implemented.
- [ ] Implement catalogue validation for unique IDs, schema validity, template placeholders and banned catastrophic patterns.
- [ ] Re-run the test and verify it passes on the v0.7 catalogue.

### Task 2: Extract linux-core module from TUI core

**Files:**
- Create: `workbench.h`
- Create: `modules/linux-core/linux_core.c`
- Create: `modules/linux-core/linux_core.h`
- Modify: `main.c`
- Modify: `Makefile`
- Modify: `tests/test_selftest.sh`

**Interfaces:**
- Produces: `const Module *linux_core_module(void)`.
- Core consumes the returned module's Task array and metadata; it does not reference a global `TASKS[]` definition.

- [ ] Extend the test to assert self-test reports module ID `linux-core` and the existing 102 Actions.
- [ ] Run and verify failure before extraction.
- [ ] Move shared enums/structs/macros to `workbench.h` and all existing Actions to `modules/linux-core/linux_core.c`.
- [ ] Replace direct `TASKS[]` use in core with the active Module interface.
- [ ] Update Makefile sources and verify build/self-test pass with 102 Actions.

### Task 3: Generalize Action arguments from two to four

**Files:**
- Modify: `workbench.h`
- Modify: `main.c`
- Modify: `modules/linux-core/linux_core.c`
- Modify: `tests/test_selftest.sh`

**Interfaces:**
- Task exposes four argument slots through a fixed `TaskArg args[4]` representation.
- Command templates may use `{1}` through `{4}` only when corresponding slots are defined.

- [ ] Add self-test fixtures/expectations for `{3}` and `{4}` placeholder support.
- [ ] Run and verify failure.
- [ ] Refactor prompt/quote/substitution code to iterate over four slots.
- [ ] Keep compatibility macros for zero/one/two argument definitions and add `A3`/`A4`.
- [ ] Verify all prior Actions and new placeholder tests pass.

### Task 4: Expand system/files/text Actions

**Files:**
- Modify: `modules/linux-core/linux_core.c`
- Modify: `tests/test_selftest.sh`

**Interfaces:**
- Adds task-oriented Actions only; IDs stay namespaced by category.

- [ ] Raise catalogue to at least 190 Actions across System, Files and Text with clear missing-tool checks/fallbacks.
- [ ] Add test minimums per category and run to see threshold failure first.
- [ ] Implement Actions, then run self-test and build tests.

### Task 5: Expand process/network/storage Actions

**Files:**
- Modify: `modules/linux-core/linux_core.c`
- Modify: `tests/test_selftest.sh`

**Interfaces:**
- Network-changing or remote Actions use Sensitive risk; destructive process/storage Actions use Privileged risk.

- [ ] Raise catalogue to at least 275 total Actions across Process, Network and Storage.
- [ ] Add category minimum tests first and verify failure.
- [ ] Implement Actions with `command -v` guards where utilities are optional.
- [ ] Re-run catalogue and build tests.

### Task 6: Expand permission/archive/user Actions to release target

**Files:**
- Modify: `modules/linux-core/linux_core.c`
- Modify: `tests/test_selftest.sh`

**Interfaces:**
- Final linux-core exports at least 300 Actions.

- [ ] Add final count/category floor assertions and verify failure.
- [ ] Add Permission, Archive and User Actions until the catalogue exceeds 300.
- [ ] Run self-test and build tests.

### Task 7: Documentation and release packaging

**Files:**
- Modify: `README.md`
- Modify: `Makefile`
- Create: `CHANGELOG.md`
- Create: `tests/smoke_tui.py`

**Interfaces:**
- `make test` runs catalogue/build tests and TUI smoke test where PTY support exists.
- `make package` produces a release tarball containing source, module source, tests, README, changelog, and built binary.

- [ ] Add a PTY smoke test that starts `wb`, checks for the Workbench desktop text, then exits cleanly.
- [ ] Add `test` and `package` Makefile targets.
- [ ] Update README architecture/count/boundaries/build/test instructions and add changelog entry.
- [ ] Run `make clean && make && make test` and verify zero warnings/all tests pass.
- [ ] Build the release archive.
