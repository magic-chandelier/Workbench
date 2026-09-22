# Workbench Plugin Format v3

Workbench plugins are directory-based and database-free. A plugin is installed while its directory exists under a plugin root, and manual removal of that directory is a complete program uninstall.

## Plugin roots

```text
/usr/local/share/workbench/plugins/   # system plugins
~/.local/share/workbench/plugins/     # current-user plugins
```

Each immediate child directory is a plugin candidate. The directory name must exactly match the manifest `id`. User plugins take precedence over system plugins with the same ID. Invalid or incompatible user candidates do not hide a valid system plugin.

## API compatibility

Workbench v0.16.1-dev3 implements Plugin API 3 and keeps compatibility with API 1 and 2. A plugin is loadable when its declared `[api_min, api_max]` range intersects the host-supported range `1..3`.

- API 1 plugins retain Application and Command Set behavior.
- File Action contributions require a compatible plugin generation with `api_max >= 2`; API 3 plugins retain the File Action capability introduced in API 2.
- Preview Provider contributions require a compatible plugin generation with `api_max >= 3`.
- A plugin may declare a wider range when it genuinely supports multiple generations.

## Directory layout

```text
plugin.example/
├── plugin.wbp
├── bin/
│   └── example
├── command-sets/
│   └── example.wbc
├── file-actions/
│   └── open-with-example.wba
└── preview-providers/
    └── file-preview.wbp
```

A plugin may provide an Application entry, Command Sets, File Actions, or any combination supported by its API range.

## Manifest

`plugin.wbp` is a Workbench-owned UTF-8 key/value format:

```text
WORKBENCH_PLUGIN=1
format=1
id=plugin.example
version=2.0.0
api_min=1
api_max=3
name_zh=示例插件
name_en=Example Plugin
description_zh=示例 Workbench 插件
description_en=Example Workbench plugin
entry=bin/example
command_sets=command-sets
file_actions=file-actions
preview_providers=preview-providers
```

Required fields are `WORKBENCH_PLUGIN`, `format`, `id`, `version`, `api_min`, `api_max`, `name_zh`, `name_en`, `description_zh`, and `description_en`. `entry` is optional. `command_sets` defaults to `command-sets`; `file_actions` defaults to `file-actions`; `preview_providers` defaults to `preview-providers`.

Plugin IDs may contain only ASCII letters, digits, `.`, `_`, and `-`. Relative paths must not contain absolute paths, `.`/`..` path components, or control characters.

## Applications

If `entry` is present, Workbench exposes the plugin under dynamic `Applications`. Core does not load third-party code into its address space. The entry runs as a separate child process/process group. Workbench revalidates the plugin manifest and entry immediately before launch and uses no-follow filesystem traversal.

Workbench does not change into the plugin directory before launch. A plugin that needs resources next to itself must self-locate rather than assuming the current working directory is its root.

## File Actions (API 2)

A plugin that supports API 2 may place `.wba` descriptors in its `file_actions` directory. Example:

```text
WORKBENCH_FILE_ACTION=1
format=1
id=extract-here
name_zh=解压到这里
name_en=Extract here
entry=bin/archive-tool
target=file
extensions=7z,zip,tar.gz
```

Fields are strict and all are required:

- `WORKBENCH_FILE_ACTION=1`
- `format=1`
- `id`: action token, using the same bounded token syntax as plugin IDs
- `name_zh` / `name_en`: menu labels
- `entry`: executable path relative to the plugin root
- `target`: `file`, `directory`, or `any`
- `extensions`: comma-separated filename suffixes without the leading dot, or `*`; matching is case-insensitive. Multi-part suffixes such as `tar.gz` are supported.

Workbench Files keeps its built-in menu product-agnostic. Matching plugin actions are appended after the built-in file operations. The Core does not know whether an action belongs to 7-Zip, an image editor, a checksum utility, or another product.

Execution protocol:

```text
entry --workbench-file-action <action-id> -- <absolute-selected-path>
```

The selected path is one argv element and is never concatenated into a shell command by Core.

Immediately before execution Workbench refreshes the plugin registry, finds the action again by `(plugin id, action id)`, verifies that the selected path still matches, reopens and reparses the `.wba` descriptor, verifies that its metadata has not changed, rechecks the executable with no-follow traversal, and finally launches the plugin as a separate process.

## Files Preview Providers (API 3)

An API 3 plugin may place strict `.wbp` descriptors in `preview_providers`:

```text
WORKBENCH_FILE_PREVIEW=1
format=1
id=archive-browser
name_zh=压缩包预览
name_en=Archive Preview
entry=bin/archive-tool
extensions=7z,zip,tar.gz
```

All fields are required. `extensions` follows the same case-insensitive suffix grammar as File Actions. The provider uses separate `--workbench-preview-list` and `--workbench-preview-materialize` argv protocols documented in `docs/EXTENSION_API.md`. Workbench Files owns the read-only browser UI; a provider supplies metadata and materializes one requested member at a time.

Preview v3 does not expose rename, move, delete, chmod, or archive write-back. Those operations require a future transactional archive-edit contract rather than pretending an archive is an ordinary writable directory.

## Plugin Command Sets

`.wbc` files in `command_sets` use the same Workbench Command Set format as user sets and appear as `[Plugin] [Read-only]`. From the desktop they use EXECUTE behavior; from Workbench Terminal they use INSERT behavior. Plugin command templates may use `{plugin_root}`; Core replaces it at load time with a shell-quoted absolute plugin root, so the same command set works from either user or system plugin roots.

## Enable / Disable

The disabled state is a regular `.workbench-disabled` file directly inside the plugin directory. Disabled plugins remain visible in `Settings → Plugins` but do not contribute Applications, Command Sets, File Actions, or Preview Providers. Launch paths re-check the disabled state, so stale in-memory objects cannot bypass a later disable.

## Uninstall

Manual deletion of the plugin directory is always sufficient. The in-product Plugin Manager first atomically renames a plugin to `.wb-removing-*` and then removes it using fd-relative no-follow operations. Symlinks inside a plugin are unlinked as links and are not followed outside the plugin directory.

## Security and licensing boundary

A plugin runs with the Workbench user's permissions. Process separation is not a sandbox; installing an untrusted plugin is equivalent to running untrusted software as that user.

Workbench Core does not adapt itself to Docker, 7-Zip, Nginx, or another individual product. Product detection, optional third-party runtime acquisition, compatibility logic, and licensing obligations belong to the plugin. Core does not `dlopen()` plugin code.
