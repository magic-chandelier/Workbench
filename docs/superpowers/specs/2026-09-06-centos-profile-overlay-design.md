# CentOS Profile Overlay Design

## Goal

Add an integrated CentOS profile to Workbench while preserving Linux Generic as the default visible experience. CentOS-specific actions remain hidden unless the effective profile is CentOS, and CentOS actions appear mixed into the existing task/category UI with a lightweight source label.

## Supported profiles

- `generic`: Linux Core only. This is the default for new installs and preserves v0.8.0 behavior.
- `centos`: Linux Core plus the CentOS overlay.
- `auto`: Detect `/etc/os-release`; use CentOS only when `ID=centos`, otherwise fall back to Generic.

The configured profile is persisted as `linux_profile=generic|centos|auto` in `~/.config/workbench/config`.

## Detection

Read `/etc/os-release`, parse `ID` and `VERSION_ID`, and recognize CentOS only for an exact `ID=centos` match. Unknown distributions fall back to Generic. The detector exposes the detected name/version to the settings UI. Tests can point detection at a fixture path through a small detector API rather than relying on the host OS.

## Overlay model

Linux Core remains an independent module with its 374 actions. A new `modules/centos/` module contains only CentOS-specific or CentOS-override actions.

At runtime Workbench builds one effective task set:

1. Start with every Linux Core action.
2. If effective profile is CentOS, for each CentOS action:
   - replace a Linux Core action with the same `id`; or
   - append it if the `id` is new.
3. Present the result as one integrated workbench.

CentOS-only actions carry `source_id="centos"`; Linux Core actions carry `source_id="linux-core"`. The task list appends a compact `CentOS` source marker only for CentOS-origin actions. Properties show the source explicitly.

## Categories

Extend categories with `Packages`, `Services`, `Firewall`, and `SELinux`. Category menus become dynamic and hide categories with zero effective actions. Therefore Generic mode does not display CentOS-only categories.

CentOS network actions using NetworkManager stay under `Network`; OS/release actions stay under `System`.

## CentOS scope

The first CentOS overlay targets modern CentOS Stream 9/10 administration patterns:

- DNF and RPM package management
- systemd/systemctl service control and journalctl logs
- firewalld/firewall-cmd
- SELinux inspection and common administration tools
- NetworkManager/nmcli
- CentOS/RHEL-family release/repository/system inspection

CentOS 7/YUM-era compatibility is not treated as the primary implementation. A legacy profile can be added later without changing the overlay mechanism.

## Safety

Use the existing three risk classes. Read-only inspection is Normal, network/state changes are Sensitive when appropriate, and package removal, service state changes, firewall permanent changes, SELinux state changes, and other administrative actions are Privileged when they can materially change the system.

Existing destructive path protection remains unchanged. Self-test validates the Generic and CentOS effective catalogues independently, including duplicate/override semantics, placeholders, shell syntax, risk/category validity, and source metadata.

## UI

Settings adds `Linux version` above confirmation/autostart controls. Selecting it opens a profile selector with `Auto detect`, `Linux Generic`, and `CentOS`. Auto mode displays detected and effective profiles.

Desktop remains a single Linux workbench entry. Its title/description reflects the effective profile: Generic shows the Linux generic command set, while CentOS shows a CentOS-enhanced Linux command set and the effective action count.

## Version

Release as Workbench v0.9.0.
