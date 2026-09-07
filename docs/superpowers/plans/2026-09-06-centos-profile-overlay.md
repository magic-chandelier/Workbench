# CentOS Profile Overlay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a persistent Generic/Auto/CentOS Linux profile with an integrated CentOS overlay and hidden CentOS-only categories in Generic mode.

**Architecture:** Linux Core stays immutable. A CentOS module contributes overlay actions, and the app constructs an effective in-memory task catalogue by replacing matching IDs and appending new CentOS actions. Category visibility, search, task display, and execution all operate on that effective catalogue.

**Tech Stack:** C11/POSIX C, POSIX shell tests, Python PTY smoke test, libc-only runtime.

**Spec:** `docs/superpowers/specs/2026-09-06-centos-profile-overlay-design.md`

## Global Constraints

- New installs default to `linux_profile=generic`.
- `auto` recognizes only exact `/etc/os-release` `ID=centos`; otherwise it falls back to Generic.
- Generic profile must preserve the existing 374 Linux Core actions and hide CentOS-only categories.
- CentOS profile must present one integrated catalogue, not a second desktop module.
- CentOS task origin must be visible in task properties and compactly marked in task rows.
- Runtime dynamic dependencies remain libc-only.
- Release version is 0.9.0.

---

### Task 1: Profile configuration and detection

**Files:**
- Modify: `workbench.h`
- Modify: `main.c`
- Create: `tests/test_linux_profile.sh`
- Create: `tests/fixtures/os-release-centos`
- Create: `tests/fixtures/os-release-ubuntu`

**Interfaces:**
- Produces: `LinuxProfile`, `DetectedLinux`, config persistence for `linux_profile`, and pure os-release parsing used by self-test/settings.

- [ ] Add failing shell assertions for default Generic, explicit CentOS, and auto detection fixtures.
- [ ] Run the new test and verify it fails because profile support is absent.
- [ ] Implement the profile enum, config persistence, os-release parser, effective-profile resolution, and a self-test/profile diagnostic path.
- [ ] Re-run the profile test and full baseline suite.

### Task 2: CentOS overlay catalogue and effective workset

**Files:**
- Create: `modules/centos/centos.h`
- Create: `modules/centos/centos.c`
- Create: `modules/centos/actions/packages.inc`
- Create: `modules/centos/actions/services.inc`
- Create: `modules/centos/actions/firewall.inc`
- Create: `modules/centos/actions/selinux.inc`
- Create: `modules/centos/actions/network.inc`
- Create: `modules/centos/actions/system.inc`
- Modify: `workbench.h`
- Modify: `main.c`
- Modify: `Makefile`
- Create: `tests/test_centos_overlay.sh`

**Interfaces:**
- Produces: `centos_module()`, source metadata on every Task, and an effective task catalogue builder with ID replacement semantics.

- [ ] Add failing tests proving Generic remains 374, CentOS is larger, CentOS-only IDs are absent in Generic, and same-ID overlay does not duplicate.
- [ ] Run and confirm failure.
- [ ] Add CentOS module/action data and source metadata.
- [ ] Implement effective catalogue build and replace direct module-task assumptions in search/count/execution paths.
- [ ] Run overlay and existing catalogue tests.

### Task 3: Dynamic categories and mixed source display

**Files:**
- Modify: `workbench.h`
- Modify: `main.c`
- Create: `tests/test_profile_categories.sh`

**Interfaces:**
- Consumes: effective task catalogue and source metadata.
- Produces: hidden zero-count categories, CentOS source labels, and profile-aware desktop/category headers.

- [ ] Add failing tests for Packages/Services/Firewall/SELinux visibility by profile and source-label strings.
- [ ] Run and confirm failure.
- [ ] Extend category enum/name/description tables and build the category menu from only non-empty effective categories plus All.
- [ ] Add compact source marker in task rows and explicit source in properties.
- [ ] Make desktop and task headers profile-aware.
- [ ] Re-run category/layout/search tests.

### Task 4: Settings UI and profile switching

**Files:**
- Modify: `main.c`
- Modify: `tests/smoke_tui.py`
- Create: `tests/test_profile_config_persistence.sh`

**Interfaces:**
- Consumes: `AppConfig.linux_profile` and effective catalogue rebuild.
- Produces: interactive profile selector and immediate rebuild after profile changes.

- [ ] Add persistence test and PTY smoke expectations for the new setting.
- [ ] Run and confirm failure.
- [ ] Add the Linux version row and profile selector UI showing configured/detected/effective profile.
- [ ] Rebuild effective tasks immediately after settings changes.
- [ ] Re-run persistence and TUI smoke tests.

### Task 5: Dual-profile self-test, docs, and release packaging

**Files:**
- Modify: `main.c`
- Modify: `tests/test_selftest.sh`
- Modify: `tests/test_release_metadata.sh`
- Modify: `README.md`
- Modify: `CHANGELOG.md`
- Modify: `Makefile`

**Interfaces:**
- Produces: v0.9.0 release metadata, dual-profile self-test output, and packaged source/binary.

- [ ] Add failing assertions that self-test validates Generic and CentOS catalogues and release metadata says 0.9.0.
- [ ] Run and confirm failure.
- [ ] Generalize catalogue validation to both effective profiles and document profile/overlay behavior.
- [ ] Update version/package metadata and Makefile source/action discovery.
- [ ] Run `make clean && make && make test && make package`.
- [ ] Extract the package into a fresh directory, rebuild, run `./wb --self-test`, and run `ldd ./wb` to verify libc-only runtime dependencies.
