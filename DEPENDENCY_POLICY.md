# Dependency Admission Policy

Workbench aims to keep its core independently implemented, small, auditable,
and license-controllable. New dependencies are evaluated according to the
following policy before they enter the source tree, link step, or release
artifacts.

## Preferred

1. Workbench-owned implementation.
2. Standard C / POSIX interfaces.
3. Normal Linux userspace APIs and system calls.
4. Executing a program already installed by the host operating system when a
   system tool is the appropriate abstraction.

## No-vendor default

Workbench does not vendor third-party source code or third-party binaries by
default. A proposed exception requires an explicit license review, provenance
record, maintenance owner, security/update plan, and update to
`THIRD_PARTY_NOTICES.md`.

## Review gates

| Dependency type | Default policy |
|---|---|
| Permissive source (for example BSD/MIT/0BSD) | Review required before copying or linking; retain required notices. |
| Apache-2.0 source/library | Review required, including NOTICE/patent obligations where applicable. |
| LGPL library | Review required; prefer dynamic/system linkage and document redistribution obligations. |
| GPL source or library | Do not add to Workbench core by default. Requires an explicit architecture and licensing decision before inclusion. |
| AGPL source or library | Do not add to Workbench core by default. Requires an explicit architecture and licensing decision before inclusion. |
| External GPL/AGPL command already installed by the host | May be invoked as a separate process after functional/security review; do not bundle it. |
| third-party binaries in Workbench release assets | Prohibited by default; explicit approval and license-compliance plan required. |
| copied third-party manuals/documentation | Prohibited by default; write original Workbench documentation or quote only within applicable license/copyright limits. |

## Required review record

Before adding any bundled or link-time dependency, document:

- upstream project and canonical source;
- exact version/commit;
- license/SPDX identifier(s);
- whether source, object code, static library, shared library, or binary is
  distributed;
- required attribution/NOTICE/source-offer obligations;
- security/update ownership;
- why a Workbench-owned implementation or separate host process is not
  preferable.

## Release checks

Release packaging must keep source and architecture-specific binaries separate.
The source archive must not contain a prebuilt `wb` binary. Any future bundled
third-party material must be visible in the archive inventory and documented in
`THIRD_PARTY_NOTICES.md` before release.
