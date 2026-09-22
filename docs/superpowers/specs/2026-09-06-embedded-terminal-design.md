# Workbench Embedded Terminal Design

## Goal
Add a first-party embedded terminal application to the Workbench desktop without surrendering the real TTY to the child shell. Workbench remains responsible for drawing UI, mouse input, short-lived scrollback, settings, and command-palette integration while a user shell runs behind a PTY.

## Desktop
Desktop order becomes:
1. Workbench Files
2. Terminal
3. Linux Generic / CentOS Command Set
4. Settings
5. Exit Workbench

Terminal is a Workbench application. Leaving it returns to the desktop and does not terminate a running shell.

## Shell / PTY model
- Create a PTY with `posix_openpt`, `grantpt`, `unlockpt`, `ptsname`, `setsid`, `TIOCSCTTY`, and `dup2`.
- Child executes the user's `$SHELL` directly with an interactive argument. No shell-string interpolation is used.
- Invalid/unexecutable `$SHELL` falls back to `/bin/sh`.
- Workbench keeps the real terminal in its own raw/mouse mode and polls both STDIN and the PTY master.
- The shell process continues when the user returns to the Workbench desktop; explicit End Shell terminates the child but preserves in-memory terminal history until Workbench exits or history is cleared.

## Terminal view and input
- Workbench always draws terminal chrome and the captured shell output.
- Mouse toolbar: Commands, Input, Older, Newer, Clear, End Shell/Restart Shell, Back.
- Clicking the terminal body focuses input forwarding to the PTY.
- Physical keyboard input is forwarded to the PTY in normal terminal mode.
- `Ctrl+]` opens a Workbench terminal control menu so keyboard-only users can reach Commands, history navigation, Clear, End/Restart, and Back without stealing common shell shortcuts.
- If the global virtual keyboard setting is enabled, the Input toolbar action opens the existing Workbench text input/virtual-keyboard component and sends the accepted line plus newline to the PTY.

## Command palette integration
- Terminal Commands opens a Workbench task picker over the current effective Linux profile.
- Building a selected task reuses the existing task argument prompts and shell-safe template rendering.
- The generated command is inserted into the PTY without a trailing newline. The user can edit it and explicitly run it by pressing Enter or using a Run toolbar control.
- The picker never silently bypasses existing Workbench task argument validation.

## Short-lived history
- History is memory-only and never automatically written to disk.
- Default maximum: 1000 logical output lines.
- When the limit is exceeded, oldest lines are discarded.
- Default history navigation chunk: 50 lines.
- Wheel-up / Older loads 50 older lines per action; Wheel-down / Newer moves toward the live bottom by the same configured amount.
- Reopening Terminal does not clear captured output by default.
- A setting controls whether opening Terminal clears the captured terminal screen/history.
- Manual Clear is always available.

## Settings
Add three settings with dual keyboard/mouse operation:
- Terminal log lines: default 1000, allowed 100..10000, adjustable without requiring the virtual keyboard.
- Terminal history load lines: default 50, allowed 10..500, adjustable without requiring the virtual keyboard.
- Clear terminal on open: default Disabled.

These are persisted in the existing Workbench config and old configs receive safe defaults.

## Renderer scope for v0.14.0
The first terminal core is deliberately small and libc/POSIX-only. It supports normal shell text, UTF-8 pass-through for display, CR/LF, backspace, tab expansion, and consumes common ANSI CSI/OSC style sequences so colored prompts do not leak escape bytes into Workbench UI. It handles basic clear-line/clear-screen semantics. Full xterm/vt100 compatibility for programs such as vim/top/tmux is a later compatibility milestone; architecture keeps the PTY and renderer separate so that parser can grow without changing the desktop or session model.

## Safety and dependency constraints
- No ncurses, readline, libvterm, libtsm, or other terminal-emulator dependency.
- No third-party terminal source is vendored.
- No shell interpolation is used to choose/launch `$SHELL`.
- Scrollback is bounded by configured line count and maximum per-line storage.
- Workbench cleanup terminates/reaps its PTY child and restores the real terminal.

## Verification
Tests must cover PTY start/stop, shell fallback, scrollback limit, 50-line default history navigation, config defaults/persistence, desktop order, reopen preservation, clear-on-open behavior, mouse toolbar flow, keyboard-only control menu, and command insertion. Existing Linux/CentOS, Files, mouse-only, keyboard-only, packaging, and licensing tests remain green.
