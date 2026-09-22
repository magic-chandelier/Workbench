# Plugin System v0.1 on Workbench v0.15.1 Design

## Baseline rule
The uploaded Workbench source is the only implementation baseline. Its existing `make test` suite is an immutable regression suite: Files, Locations, Terminal, Command Sets, Settings, Linux Generic/CentOS overlays, keyboard, mouse and virtual-keyboard behavior must remain available.

## Core boundary
Core only discovers, validates, lists, launches and uninstalls plugins. It contains no Docker/7Z/Nginx/etc. branches and gains no third-party runtime dependency. Plugins are directories under `/usr/local/share/workbench/plugins` (system) or `~/.local/share/workbench/plugins` (user). A valid `plugin.wbp` is required; discovery never executes plugin code.

## Manifest v1
Required keys: `WORKBENCH_PLUGIN=1`, `format=1`, `id`, `version`, `api_min`, `api_max`, `name_zh`, `name_en`, `description_zh`, `description_en`. Optional `entry` is a relative executable path below the plugin root. `command_sets` defaults to `command-sets`. IDs use `[A-Za-z0-9._-]+`; directory name must equal id. Workbench Plugin API is 1.

## Precedence and isolation
System plugins are scanned first, user plugins second. A valid compatible user plugin shadows a same-id system plugin. Invalid/incompatible user candidates do not shadow a valid system plugin. `.wb-removing-*` and symlink candidate directories are ignored. One bad plugin never aborts scanning other plugins.

## Applications and execution
A plugin with a valid executable `entry` is shown under a dynamic Applications desktop item. Workbench never `dlopen()`s plugin code. It launches the entry as a child process/process group after revalidating the manifest and entry, temporarily restores normal terminal mode for the child, then restores Workbench raw/mouse/cursor state after exit. Plugin crash/exit affects only that plugin invocation.

## Plugin Command Sets
`.wbc` files under the plugin command-set directory use the existing Workbench command-set format. They are loaded as `COMMAND_SET_PLUGIN`, read-only, and are browsed through the same Command Sets browser used by the homepage and Terminal INSERT path. Removing the plugin removes those sets after registry reload. Plugin set internal IDs are namespaced to avoid collisions with user/system set IDs.

## Uninstall
No install database exists. Manual directory deletion is sufficient. In-product uninstall requires confirmation. User plugins are atomically renamed to `.wb-removing-<id>-<pid>` inside the same plugin root and recursively deleted with fd-relative, no-follow operations. System plugins use the same deletion primitive when Workbench is root; otherwise Workbench invokes `sudo <self> --plugin-remove-system <id>`. The privileged CLI accepts only a validated plugin ID and derives the system path itself.

## Data
Uninstall removes only the plugin program directory. It does not delete `~/.config/workbench/plugins/<id>` or `~/.local/share/workbench/plugin-data/<id>`.

## Release gate
Existing v0.15.1 tests remain unchanged and must pass. New plugin unit/PTY tests must pass. `--self-test` gains `plugins_guard=PASS`. Source package remains source-only; binary remains libc-only.
