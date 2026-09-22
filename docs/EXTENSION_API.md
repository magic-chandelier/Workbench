# Workbench Extension Registry v3

v3 keeps the API v2 Files File Action path and adds a product-agnostic, read-only Files Preview Provider path. Core never loads third-party code into its own address space.

## Compatibility

- Plugin API 1: Applications and Command Sets.
- Plugin API 2: API 1 plus Files File Actions.
- Plugin API 3: API 1/2 plus read-only Files Preview Providers.
- A plugin is loadable when its declared API range intersects the host-supported range `1..3`; a contribution is accepted only when the plugin explicitly supports that contribution's API generation.

## File Actions

The Core-owned `WbExtensionRegistry` stores bounded copies of action metadata from active API 2+ plugins. Workbench Files filters by target kind and filename suffix and appends matching actions to the ordinary context menu. Execution is:

```text
entry --workbench-file-action <action-id> -- <absolute-selected-path>
```

The selected path is one argv element. Before execution Core refreshes the plugin registry, reparses the descriptor, checks that it is unchanged, rechecks the selected path, and reopens the executable through no-follow traversal.

## Files Preview Providers (API 3)

An API 3 plugin may contribute strict `*.wbp` descriptors from its `preview_providers` directory. Core stores the provider ID, localized labels, executable entry, extension filter, source plugin ID, and descriptor path.

List protocol:

```text
entry --workbench-preview-list <provider-id> -- <absolute-container-path>
```

The provider writes zero or more UTF-8-independent records to stdout:

```text
E<TAB>F<TAB><size><TAB><hex-encoded-member-path>
E<TAB>D<TAB>0<TAB><hex-encoded-member-path>
```

Core caps captured output at 16 MiB and parsed entries at 65,536. It independently rejects absolute member paths, empty components, `.`/`..` components, NUL/control characters, malformed hex, and overlong paths.

Materialize protocol:

```text
entry --workbench-preview-materialize <provider-id> -- <absolute-container-path> <member-path> <absolute-destination>
```

The destination is chosen by Core. Preview providers are expected to create it exclusively and must not overwrite an existing path. Workbench Files uses materialization only when a user actually views or copies a member.

The preview UI is Core-owned Workbench Files UI. Navigation, keyboard/mouse input, properties, hidden-file filtering, text viewing, and single-file copy remain Core behavior. The virtual archive view is intentionally read-only: rename, move, delete, chmod, and in-place archive mutation are not mapped to ordinary filesystem operations.

## Security invariants

- only active compatible plugins whose `api_max` includes the feature generation contribute that extension type
- plugin/provider/action IDs and relative paths use bounded syntax
- descriptor files and executable entries are opened with no-follow traversal
- executable entries must be regular executable files
- `(source, id)` is unique per extension type in a registry snapshot
- metadata is copied into Core-owned bounded storage
- all user paths are argv elements, never shell-concatenated by the extension launcher
- descriptor and plugin state are revalidated before execution/materialization
- preview stdout is bounded before parsing
- Core validates virtual member paths independently of the provider
- third-party code is never `dlopen()`ed into Workbench Core
