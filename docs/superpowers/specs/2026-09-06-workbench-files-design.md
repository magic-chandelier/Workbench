# Workbench Files Design

## Goal

Add a first-party, libc/POSIX-only local file manager named **Workbench Files** as the first item on the Workbench desktop, above the Linux command workbench.

## Product placement

Desktop order:

1. Workbench Files
2. Linux Generic / CentOS Linux Command Set
3. Settings
4. Exit Workbench

Workbench Files is an independent desktop application, not a Linux Action category and not a CentOS overlay. Linux profile selection does not affect its availability.

## V0.10 scope

Workbench Files v1 provides local filesystem management for terminal/server use:

- browse directories, including parent navigation
- sort directories before non-directories, then by name
- toggle hidden files
- inspect file properties: path, type, size, owner/group IDs, mode, timestamps, symlink target when applicable
- built-in read-only text viewer for regular files
- create file
- create directory
- copy file/directory/symlink recursively
- move/rename file/directory/symlink
- delete file/symlink
- recursively delete directory only after explicit confirmation
- change mode using octal permissions
- keyboard navigation integrated with Workbench (`Up/k`, `Down/j`, `Enter`, `Backspace/Left`, `q/Esc`)

Out of scope for v0.10:

- archive-as-folder browsing
- remote/SFTP/FTP filesystems
- image/media preview
- plugin previewers
- full text editor
- ACL/xattr editor
- ownership changes
- trash/recycle bin
- multi-selection/batch operations

## Safety model

File operations use POSIX/Linux filesystem APIs directly. Paths never pass through `/bin/sh`.

- symbolic links are treated as links and are not followed for recursive copy/delete
- recursive delete rejects `/`, `.`, `..`, and any path resolving to `/`
- the synthetic `..` row cannot be renamed, copied, moved, deleted, or chmodded
- create operations reject empty names, `.`/`..`, names containing `/`, and collisions
- copy rejects copying a directory into itself or its descendants
- move uses `rename(2)` first; if `EXDEV`, it performs copy-then-remove using the same safe primitives
- directory delete always asks for explicit confirmation regardless of command-confirmation settings
- delete of a regular file/symlink also asks for explicit confirmation in the file-manager UI
- chmod validates exactly 3 or 4 octal digits and applies to the selected entry only

## Architecture

### `modules/files-manager/files_manager.h/.c`

Owns filesystem data/model and mutations. It does not draw terminal UI and does not run shell commands.

Key public types/functions:

- `FmEntry`, `FmDirectory`
- `fm_load_directory()` / `fm_free_directory()`
- `fm_join_path()` / `fm_parent_path()`
- `fm_create_file()` / `fm_create_directory()`
- `fm_copy_path()` / `fm_move_path()` / `fm_remove_path()`
- `fm_chmod_path()`
- `fm_type_name()`

### `main.c`

Owns Workbench Files UI because it already owns terminal raw mode, key parsing, localization, prompts, and confirmation screens. It calls only the filesystem module for mutations and directory loading.

The file manager starts in the process current working directory. If unavailable it falls back to `$HOME`, then `/`.

## UI

Directory screen shows:

- current path
- controls
- hidden-files state
- rows with name, type, size, mode
- `..` as the first row when not at `/`

Commands:

- `Enter`: enter directory; for regular file open viewer; for symlink to directory, follow it for browsing only
- `Left` / `Backspace`: parent directory
- `v`: view selected regular file
- `p` / `Right`: properties
- `n`: create regular file
- `N`: create directory
- `c`: copy selected entry; prompt for destination path
- `m`: move selected entry; prompt for destination path
- `r`: rename selected entry; prompt for new name
- `d`: delete selected entry with confirmation
- `x`: chmod selected entry; prompt for octal mode
- `h`: toggle hidden entries
- `l`: toggle interface language and persist normal Workbench setting
- `q` / `Esc`: return to desktop

Text viewer is read-only, bounded-memory, line/page oriented. It must not load an entire large file into RAM. Binary files are detected by a NUL byte in the initial chunk and show a message instead of printing raw binary to the terminal.

## Compatibility

- C/POSIX/libc only, no ncurses/readline/GLib/Go/Rust/Python runtime dependencies
- preserve the single `wb` binary
- no third-party file-manager source code
- Linux Generic and CentOS Action counts remain 374 and 483 respectively

## Verification

Add automated tests for:

- directory listing and hidden filtering
- create file/directory
- recursive copy preserving symlinks as symlinks
- rename/move
- recursive delete
- copy-self/descendant guard
- chmod validation/application
- desktop displays Workbench Files above command workbench
- PTY can open Workbench Files and return to desktop
- existing v0.9.1 self-test, profile tests, Action counts and TUI profile switching continue to pass
- release archive rebuilds and tests cleanly
