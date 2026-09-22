# Plugin System v0.1 on Workbench v0.15.1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Add Plugin System v0.1 to the uploaded Workbench baseline without removing or replacing any v0.15.1 feature.

**Architecture:** Add a focused `modules/plugins` module. Extend the existing Command Sets model only enough to register read-only plugin sets. Keep all existing UI primitives and app implementations; add Applications and Settings > Plugins as generic views over the plugin registry.

**Tech Stack:** C11, libc, POSIX/Linux APIs, existing Python PTY smoke harnesses.

**Spec:** `docs/superpowers/specs/2026-09-16-plugin-system-v0.1-on-v0151-design.md`

## Global Constraints
- Existing `make test` suite is immutable regression coverage.
- No plugin-specific product names or compatibility logic in Core.
- No new shared-library dependency.
- No `dlopen()` plugin execution.
- Manual plugin-folder deletion remains a complete uninstall.

---

### Task 1: Registry and manifest
- [ ] Add failing C tests for manifest parsing, dual-root discovery, precedence, invalid isolation, manual removal.
- [ ] Implement `modules/plugins/plugins.h/.c` with API v1 parsing and registry scan.
- [ ] Re-run plugin tests and complete old `make test`.

### Task 2: Safe runtime and uninstall
- [ ] Add failing tests for entry traversal/symlink rejection, normal/crash exit isolation, safe recursive uninstall and symlink no-follow.
- [ ] Implement child process-group launch and atomic rename + fd-relative removal.
- [ ] Add validated system-removal CLI primitive.

### Task 3: Plugin Command Sets
- [ ] Add failing tests for read-only plugin `.wbc` load and removal.
- [ ] Extend CommandSetStore capacity and parser to register namespaced plugin sets without changing user/system semantics.
- [ ] Keep all existing Command Sets tests green.

### Task 4: Applications and Plugin Manager UI
- [ ] Add failing PTY tests for dynamic Applications, plugin launch, keyboard and mouse Plugin Manager uninstall.
- [ ] Reuse existing raw-mode/menu/hit-test/confirmation primitives; add no second UI framework.
- [ ] Add Settings > Plugins and dynamic Desktop Applications item.

### Task 5: Self-test, docs and release
- [ ] Add plugin guard to self-test and plugin tests to Makefile.
- [ ] Update version to Workbench 0.16.0 / Plugin System 0.1.0, README, CHANGELOG, plugin format docs and example plugin.
- [ ] Run unchanged v0.15.1 suite plus new suite, sanitizers, libc dependency check, source-only package extraction/rebuild/retest.
