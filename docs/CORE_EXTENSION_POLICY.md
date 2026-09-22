# Workbench Core and Extensions

Workbench Core stays small and product-agnostic. Optional product behavior belongs to plugins rather than branches in `main.c` for individual products.

Core owns file browsing and safe file operations, terminal/settings, command-set infrastructure, plugin discovery/lifecycle, generic File Actions, and generic read-only Files Preview Provider plumbing.

Workbench v0.16.1-dev2 introduced end-to-end generic Files File Actions. v0.16.1-dev3 adds the first generic Preview Provider consumer: a plugin can describe a read-only virtual file tree while Workbench Files keeps ownership of navigation, input, presentation, properties, and on-demand materialization.

Core must not add checks such as `if 7z exists`, `if docker exists`, or product-specific archive handling. A 7-Zip plugin is only one consumer of the same interfaces that future archive, document, media, checksum, or other plugins may use.

Archive mutation is deliberately outside Preview Provider v3. Rename/move/delete/chmod cannot be truthfully mapped to an archive without format-aware transactional rebuild, verification, replacement, cancellation, and recovery semantics. A future Archive Edit/Export API may define those semantics independently.

Runtime-provider abstractions also remain outside v3; product runtime acquisition and licensing obligations stay inside each plugin.
