# Workbench GitHub Release & Licensing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship v0.13.0 as a GitHub-ready source-available release with a pure source archive, explicit third-party boundaries, a dependency policy, and README component inventory.

**Architecture:** Keep Workbench code and binary unchanged functionally. Change release metadata and packaging, add project-policy/legal documentation, and add regression checks that enforce the release boundary. Host-installed tools remain separate processes and are not vendored.

**Tech Stack:** C/POSIX, Make, shell tests, Markdown documentation.

**Spec:** `docs/superpowers/specs/2026-09-06-github-release-licensing-design.md`

## Global Constraints

- Preserve libc/POSIX-only runtime direction and existing 374 Generic / 483 CentOS Action catalogues.
- Source archive must not contain the compiled `wb` executable.
- External system programs must not be represented as bundled Workbench code.
- No third-party source or binary is added in this release.
- Existing TUI, Files, dual-input, Profile and safety behavior must remain unchanged.

---

### Task 1: Release-boundary regression tests

**Files:**
- Create: `tests/test_release_policy.sh`
- Create: `tests/test_source_package.sh`
- Modify: `Makefile`

**Interfaces:**
- Produces: `package-only` target that creates `dist/workbench-v$(VERSION)-source.tar.gz` without running the full suite recursively.

- [ ] Write `tests/test_release_policy.sh` to require license/policy files, README component inventory, trademark disclaimer, and v0.13.0 metadata.
- [ ] Run it and verify failure because the new files/version do not exist yet.
- [ ] Add a non-recursive `package-only` target and `tests/test_source_package.sh` that creates/extracts the archive and rejects a packaged top-level `wb`.
- [ ] Run it and verify failure against the old packaging behavior.

### Task 2: License and upstream-boundary documents

**Files:**
- Create: `LICENSE`
- Create: `THIRD_PARTY_NOTICES.md`
- Create: `DEPENDENCY_POLICY.md`
- Create: `.gitignore`

**Interfaces:**
- Produces: Workbench Source License 1.0 and the policy/notice texts required by Task 1.

- [ ] Add the source-available Workbench Source License 1.0 with private-use grant, commercial/redistribution restrictions, trademark exclusion, warranty disclaimer, third-party carve-out, and contribution grant.
- [ ] Add THIRD_PARTY_NOTICES.md separating platform runtime from external host programs and recording representative upstream license families.
- [ ] Add DEPENDENCY_POLICY.md enforcing no-vendor-by-default and review gates for permissive/LGPL/GPL/AGPL code.
- [ ] Add `.gitignore` for `wb`, `dist/`, editor/OS noise.
- [ ] Run `tests/test_release_policy.sh` and verify only version/README/package items remain failing.

### Task 3: README component inventory and trademark wording

**Files:**
- Modify: `README.md`

**Interfaces:**
- Produces: `## 组件、外部工具与许可证边界` section and CentOS non-affiliation notice.

- [ ] Add the relationship table covering Linux/POSIX/libc, generic external tools, and CentOS-profile tools.
- [ ] Explicitly state that exact command implementations vary by distribution and no listed external utility is bundled by Workbench.
- [ ] Add CentOS trademark/non-affiliation wording.
- [ ] Link LICENSE, THIRD_PARTY_NOTICES.md and DEPENDENCY_POLICY.md from README.
- [ ] Run `tests/test_release_policy.sh` and verify README checks pass.

### Task 4: v0.13.0 source-only packaging

**Files:**
- Modify: `Makefile`
- Modify: `main.c`
- Modify: `CHANGELOG.md`
- Modify: `tests/test_release_metadata.sh`

**Interfaces:**
- Produces: version `0.13.0`; archive `dist/workbench-v0.13.0-source.tar.gz`.

- [ ] Bump Makefile and `WB_VERSION` to `0.13.0` and update release metadata test.
- [ ] Change packaging file list to source/tests/docs/policy files only; do not copy `wb`.
- [ ] Make `package` depend on `test` then `package-only`.
- [ ] Add v0.13.0 changelog entry describing source-only packaging and licensing boundary.
- [ ] Run release-policy and source-package tests to green.

### Task 5: Full release verification

**Files:**
- No production files unless a regression is found.

**Interfaces:**
- Consumes: all previous tasks.

- [ ] Run `make clean && make test` and require exit 0 with no compiler warnings.
- [ ] Run `make package` and inspect tar contents; require no packaged `wb`.
- [ ] Extract the source archive to a clean directory, run `make clean && make test`, and require exit 0.
- [ ] Run `./wb --self-test` from the rebuilt source release.
- [ ] Run `ldd wb` and confirm current release build has only the system C runtime/loader dependencies.
- [ ] Copy the verified source archive and x86_64 binary to `/mnt/data` and compute SHA-256 hashes.
