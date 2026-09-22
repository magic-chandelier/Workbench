# Workbench Dual-Input UI Design

## Goal

Make Workbench-owned UI workflows operable end-to-end with either a keyboard alone or a mouse alone, while preserving the libc/POSIX-only single-binary architecture. Every Workbench-owned interactive user action must expose both a keyboard path and a mouse path. Text entry must remain possible when the physical keyboard is unavailable through a first-party on-screen virtual keyboard.

## Product rule

The release introduces a project-wide interaction invariant:

> No Workbench-owned interactive feature may require a mouse, and no Workbench-owned interactive feature may require a physical keyboard.

Keyboard-only mode must complete the same workflows as mouse-only mode. Mouse support remains terminal-dependent: when the terminal does not implement SGR/xterm mouse reporting, the full keyboard path remains available.

## Scope

V0.12 covers all current interactive surfaces:

- desktop/home
- Linux/CentOS category navigation
- task list and task properties
- search
- task argument prompting
- run confirmations and generic confirmations
- Settings
- language/profile/confirmation/autostart settings
- Workbench Files Locations
- Workbench Files browser
- file properties, context menu and destructive confirmations
- Workbench Files text viewer navigation
- every current Workbench Files text prompt: create, copy, move, rename and chmod
- program exit from desktop

Out of scope:

- implementing a Chinese/Japanese/Korean input method
- clipboard integration
- touch gestures beyond mouse-compatible terminal events
- mouse support in terminals that do not report mouse events
- replacing the terminal with a graphical UI toolkit
- proxying or emulating input for arbitrary child programs after Workbench hands them the terminal (for example sudo password prompts, ssh prompts, or full-screen terminal applications)

## Architecture

### Input event layer

`main.c` currently returns raw key codes from `read_key()` and exposes mouse coordinates through global variables. V0.12 adds a small event abstraction on top of that parser:

- event kind: key, mouse press, mouse release, wheel up, wheel down, EOF
- key code / printable byte where applicable
- mouse x/y/button

The low-level SGR parser remains dependency-free. Existing raw `read_key()` callers are migrated toward reusable widgets rather than growing page-specific coordinate tests.

### Action/widget layer

Common interactive patterns become reusable helpers:

- selectable vertical menu
- clickable button row
- two-button confirmation dialog
- choice dialog / radio-style selector
- message dialog
- text input dialog

Each helper accepts keyboard shortcuts and mouse hit regions but returns a semantic result such as `UI_ACCEPT`, `UI_CANCEL`, `UI_UP`, or a selected item index. Pages no longer need separate business logic for keyboard and mouse activation.

### Text entry and virtual keyboard

All prompts use one input component. Physical keyboard input edits the buffer directly. The same screen contains an on-screen ASCII keyboard that can be clicked with the mouse.

The virtual keyboard includes:

- lowercase alphabet
- uppercase via Shift toggle
- digits
- common Linux/path/shell parameter punctuation: `/ . - _ ~ : @ + = , ; ' " [ ] ( ) { } $ % & ! # * ? | \\ < >`
- Space
- Backspace
- Clear
- Accept
- Cancel

The component edits UTF-8 buffers without interpreting text through a shell. Existing argument validators remain authoritative after submission. Existing UTF-8 text can be edited with backspace at code-point boundaries, but mouse-only composition of arbitrary CJK characters is not provided because that requires an IME.

### Mouse-only navigation

Every screen exposes clickable controls for actions that currently exist only as keyboard shortcuts. Important examples:

- Desktop: every row is clickable.
- Settings: each setting row and Back are clickable.
- Linux profile and confirmation policy: every option is clickable.
- Task detail: Run and Back are clickable.
- Task/category list: rows plus Settings/Back controls are clickable.
- Search: search field opens the common text input dialog; results are clickable.
- Confirmations: explicit Confirm and Cancel buttons.
- Files browser: existing row navigation remains, plus visible action buttons / context menu for New, Copy, Move, Rename, Delete, Properties, chmod, Locations, Home and Back.
- Text viewer: clickable Previous, Next and Back controls.

### Keyboard-only navigation

Every clickable control has a key equivalent. Existing keys remain where practical. Dialogs always support arrows/Tab-style movement, Enter to activate, and Esc/q to cancel/back. No existing mouse-only feature is introduced.

## Safety

- The input refactor does not weaken Workbench's three-level command risk model.
- Confirmation dialogs require an explicit semantic Confirm action; a mouse release or arbitrary key cannot confirm destructive work.
- Workbench Files continues to call POSIX filesystem functions directly and never interpolates file paths into shell commands.
- Virtual-keyboard text is passed through existing `validate_arg()` and file-manager validation.
- Password/secret entry is not added in this release.
- Mouse hit regions are recalculated on every redraw and cannot persist across screens.
- SGR mouse coordinates outside the current terminal layout are ignored.

## Accessibility and fallback

The UI always prints concise keyboard shortcuts even when mouse is available. Mouse controls are rendered as bracketed labels where space permits. On narrow terminals, secondary actions may live behind a clickable `More` menu, but keyboard shortcuts still remain available.

## Tests

Automated verification must include:

1. Existing full v0.11 test suite remains green.
2. Unit/smoke tests for the common text input component and virtual keyboard editing.
3. Keyboard-only PTY workflow that sends no SGR mouse sequences and can:
   - navigate desktop
   - open Settings and change/restore a setting
   - open Linux workbench and task properties
   - use search text input
   - enter Workbench Files, create/rename/chmod/delete a temporary file
   - exit
4. Mouse-only PTY workflow that sends no ordinary printable/navigation keyboard input after startup and can:
   - open Settings
   - change/restore language or profile using clicks
   - return to desktop
   - open Workbench Files
   - enter Home or a test mount/location
   - create a file using the on-screen keyboard
   - rename it using the on-screen keyboard
   - open properties
   - delete it through a clickable confirmation
   - return to desktop and exit through a click
5. Regression checks that run confirmations can be accepted/cancelled using both input methods.
6. `make clean && make` produces zero compiler warnings with `-Wall -Wextra -Wpedantic`.
7. Release archive is extracted, rebuilt and retested.
8. Runtime dependency remains libc/system loader only.

## Version

Release as Workbench v0.12.0.
