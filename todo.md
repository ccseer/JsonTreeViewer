# JsonTreeViewer Roadmap

基于 `simdjson 4.6.3`，目标不是回避大 JSON，而是利用它的高性能解析能力，把插件做成一个对”大文件更友好”的 Seer 预览器。  
但产品设计仍然要避免无界展开、无界搜索、无界导出这类会把 UI 和内存重新拖垮的操作。

## Version Milestones

### v1.1 (Current)
- [x] CMake build system
- [x] Seer-sdk 3.0 integration
- [x] simdjson 4.6.3 upgrade
- [x] Lazy loading with fetchMore

### v1.2 (Next - Target Q2 2026)
- [ ] File size detection and large-file mode
- [ ] Large array optimization with paging
- [ ] JSON Pointer display and navigation
- [ ] Copy actions and subtree export
- [ ] Byte offset caching

### v1.3 (Future)
- [ ] Memory mapping
- [ ] Visual enhancements (type coloring, icons)
- [ ] Performance metrics and diagnostics
- [ ] Threaded search

### v2.0 (Long-term)
- Advanced search with filters
- File statistics and analysis
- Clipboard comparison
- Plugin configuration UI

## Design Principles

- [ ] Use `simdjson::ondemand` as the primary parsing model.
- [ ] Keep tree loading lazy and pointer-driven.
- [ ] Prefer bounded operations over full-document materialization.
- [ ] Treat large-file support as “responsive partial exploration”, not “expand everything safely”.
- [ ] Only introduce threading after bounded loading rules and performance hotspots are clear.
- [ ] Use byte offset caching to avoid re-parsing from file start.
- [ ] Monitor memory pressure and adapt behavior accordingly.

## v1.2 Core Usability

### 0. File size detection and large-file mode (CRITICAL)

- [ ] Detect file size before loading using QFileInfo.
- [ ] Define thresholds: small (<10MB), medium (10-100MB), large (>100MB), extreme (>1GB).
- [ ] Enable large-file mode automatically for files >100MB.
- [ ] In large-file mode:
  - [ ] Limit initial expand depth to 2 levels.
  - [ ] Limit array preview to first 100 elements by default.
  - [ ] Show warning banner: "Large file detected. Showing limited preview."
  - [ ] Disable or warn on dangerous operations (Expand All, Copy Subtree on root).
- [ ] Add user preference to override thresholds (via config file or environment variable).

### 1. Byte offset caching (PERFORMANCE CRITICAL)

- [ ] Add `size_t byte_offset` field to JsonTreeItem.
- [ ] Use `raw_json_token()` to capture byte position during node creation.
- [ ] Store offset alongside pointer for each node.
- [ ] Store byte_length for subtree size estimation.
- [ ] Optimize from O(File Size) to O(1) for deep node expansion.
- [ ] Test with GB-sized files to verify no "freeze" on deep node clicks.

### 2. JSON Pointer path display

- [ ] Add a status bar at the bottom of the viewer.
- [ ] Show the currently selected node's JSON Pointer in the status bar (left side).
- [ ] Format: `Path: /store/book/0/title` or `Path: (root)` when nothing selected.
- [ ] Make the path label selectable to copy to clipboard.
- [ ] Update the path display when tree selection changes.
- [ ] Clear or reset the path display when no node is selected.
- [ ] Reuse the existing per-node `pointer` as the source of truth.
- [ ] Show byte size alongside path in stats section.

### 2.5. Status bar with statistics and file info

- [ ] Add status bar widget at the bottom of the main layout.
- [ ] Left section: JSON Pointer path (see #2).
- [ ] Middle section: Node statistics.
  - [ ] Show node size and child count.
  - [ ] Show visible node count: `1,234 nodes`.
  - [ ] Show current tree depth: `Depth: 5`.
  - [ ] Update dynamically as tree expands/collapses.
- [ ] Right section: File information.
  - [ ] Show file size: `2.3 MB`.
  - [ ] Show load time: `Loaded in 0.5s`.
  - [ ] Show large-file mode indicator: `⚠️ Large file mode` (if applicable).
- [ ] Use QLabel widgets with appropriate styling.
- [ ] Respect DPR scaling in updateDPR().

### 2.6. Context menu for tree nodes

- [ ] Add context menu to JsonTreeView (override contextMenuEvent).
- [ ] Menu items:
  - [ ] Copy Key (Ctrl+Shift+K)
  - [ ] Copy Value (Ctrl+Shift+V)
  - [ ] Copy Path (Ctrl+Shift+P) - JSON Pointer format
  - [ ] Copy Dot Path - e.g., `store.book[0].title`
  - [ ] Separator
  - [ ] Copy Subtree (Ctrl+Shift+C)
  - [ ] Export Selection to File...
  - [ ] Separator
  - [ ] Expand to Level...
  - [ ] Collapse All
  - [ ] Separator (conditional)
  - [ ] Open URL (only for http/https strings)
  - [ ] Copy as ISO 8601 (only for detected timestamps)
- [ ] Disable menu items when not applicable (e.g., "Copy Key" on root).
- [ ] Use QClipboard for copy operations.

### 2.7. Enhanced top toolbar

- [ ] Add search type selector (QComboBox or button group).
  - [ ] Options: All, Key, Value, Path, Type.
  - [ ] Default to "All" (current behavior).
  - [ ] Update filter logic based on selection (currently uses prefix parsing).
- [ ] Add large-file warning banner (QLabel with warning icon).
  - [ ] Show when large-file mode is active.
  - [ ] Text: `⚠️ Large file (500 MB). Limited preview enabled.`
  - [ ] Make dismissible or persistent based on preference.
- [ ] Consider adding "Go to Path" input field (QLineEdit).
  - [ ] Placeholder: `Enter JSON Pointer (e.g., /store/book/0)`.
  - [ ] Press Enter to navigate.
  - [ ] Show error message if path not found.

### 3. Copy actions

- [ ] Add a context menu on tree nodes.
- [ ] Add `Copy Key`.
- [ ] Add `Copy Value`.
- [ ] Add `Copy Path` (JSON Pointer format).
- [ ] Consider adding `Copy Dot Path` such as `store.book[0].title`.
- [ ] Keep these actions O(1) or close to it whenever possible.

### 4. Copy Subtree and Export Selection

- [ ] Add `Copy Subtree` for object and array nodes.
- [ ] Serialize the selected subtree as formatted JSON.
- [ ] Gate this action for oversized subtrees (e.g., >10MB or >10,000 nodes).
- [ ] Show a warning or refuse the operation when the subtree is too large.
- [ ] Use the selected node's pointer (or byte offset) as the source of truth for subtree export.
- [ ] Add `Export Selection to File...` action in context menu.
- [ ] Allow saving selected subtree directly to a .json file.
- [ ] Useful for extracting specific fragments from multi-GB log files.

### 5. Expand Path

- [ ] Add an input entry for JSON Pointer path navigation.
- [ ] Support jumping to a node by pointer such as `/store/book/0/title`.
- [ ] Expand ancestors step by step until the target node is visible.
- [ ] Select and scroll to the matched node after expansion.
- [ ] Show a clear message when the pointer does not exist.
- [ ] Keep path expansion incremental instead of forcing broad tree expansion.

### 5. Controlled expand/collapse

- [ ] Add `Collapse All`.
- [ ] Add `Expand to Level...`.
- [ ] Keep `Expand All` optional and guarded.
- [ ] Warn or no-op when `Expand All` would be too expensive (disabled for non-small files).
- [ ] Avoid full-tree materialization for common browsing actions.

### 6. Structured search

- [ ] Extend the current filter beyond plain text contains.
- [ ] Support `key:xxx`.
- [ ] Support `value:xxx`.
- [ ] Support `path:xxx`.
- [ ] Support `type:object|array|string|number|boolean|null`.
- [ ] Support direct JSON Pointer input as a jump action.
- [ ] Do not implement full JSONPath syntax in this phase.
- [ ] Keep search bounded to visible or incrementally reachable data when needed.

## Large-File First Improvements

### 7. Large array optimization

- [ ] Show array summary text like `[Array (12,345 items)]`.
- [ ] Avoid eagerly materializing huge arrays.
- [ ] Add chunked or paged loading for large arrays.
- [ ] Keep expanding a large array predictable and bounded.
- [ ] Consider range nodes such as `0-999`, `1000-1999` if needed.

### 8. Large object optimization

- [ ] Avoid eagerly materializing huge objects with many fields (>1,000 keys).
- [ ] Consider partial field loading when the field count is very high.
- [ ] Show object summary: `{Object (5,678 keys)}`.
- [ ] Optionally implement alphabetical grouping for objects with >1,000 keys (e.g., `[A-C]`, `[D-F]`).
- [ ] Keep object expansion responsive even when top-level fanout is large.

### 10. Memory pressure monitoring

- [ ] Detect available system memory using platform APIs (Windows: GlobalMemoryStatusEx).
- [ ] If file size exceeds 50% of available physical memory, show warning.
- [ ] Automatically enable "extreme large-file mode":
  - [ ] Reduce initial expand depth to 1.
  - [ ] Reduce array preview to 50 elements.
  - [ ] Disable UI animations and effects.
  - [ ] Show persistent warning banner.
- [ ] Log memory usage periodically in debug mode.

### 11. Error reporting

- [ ] Improve invalid JSON handling.
- [ ] Show parse failure details instead of only generic load failure.
- [ ] If practical, surface approximate error position such as line/column from simdjson error.
- [ ] Show a short nearby snippet or reason when parsing fails.
- [ ] Display error in a message box or status area, not just debug log.

### 12. Selected node prettify view

- [ ] Add an action to preview the selected node as formatted JSON text.
- [ ] Limit this to the selected subtree instead of replacing the full viewer mode.
- [ ] Refuse or warn on oversized subtree formatting (>10MB or >10,000 nodes).
- [ ] Keep Seer's existing Text View as the fallback full-text viewer.
- [ ] Consider opening prettified view in a separate dialog or panel.

### 13. Memory-mapped file loading

- [ ] Replace `padded_string::load()` with memory-mapped approach for large files.
- [ ] Use platform-specific mmap (Windows: CreateFileMapping/MapViewOfFile).
- [ ] Wrap mmap in RAII helper for safe cleanup.
- [ ] Create `padded_string_view` from mapped memory.
- [ ] Handle mapping failures gracefully (fall back to load for small files).
- [ ] Test with files >4GB on 64-bit systems.
- [ ] Document memory usage characteristics.
- [ ] Only enable for files >100MB to avoid overhead on small files.

### 14. File statistics and analysis

- [ ] Add "File Info" or "Statistics" action in menu.
- [ ] Use simdjson to perform a fast single-pass scan.
- [ ] Display:
  - [ ] Total node count (objects + arrays + primitives).
  - [ ] Maximum depth.
  - [ ] Type distribution (X objects, Y arrays, Z strings, etc.).
  - [ ] Largest array size.
  - [ ] Largest object key count.
- [ ] Useful for developers to quickly assess API response complexity.
- [ ] Run in background thread with progress indicator.
- [ ] Cache results to avoid re-scanning on subsequent requests.

## Visual and Helper Enhancements

### 15. Type coloring

- [ ] Color value cells by JSON type (string=green, number=blue, boolean=orange, null=gray).
- [ ] Keep the palette readable in both light and dark themes.
- [ ] Avoid using color as the only type indicator (also use icons or text).
- [ ] Make colors configurable via theme or settings.

### 16. Object and array icons

- [ ] Add distinct icons for object `{}` and array `[]` nodes.
- [ ] Keep icons subtle and consistent with Seer preview UI.
- [ ] Use Qt's built-in icon themes or custom SVG icons.

### 17. Timestamp helper

- [ ] Detect likely Unix timestamps in numeric values (e.g., 1609459200).
- [ ] Heuristic: 10-digit or 13-digit integers in reasonable date range (1970-2100).
- [ ] Show a readable datetime in tooltip or secondary text (e.g., "2021-01-01 00:00:00 UTC").
- [ ] Keep raw value unchanged in the tree.
- [ ] Add context menu action: "Copy as ISO 8601".

### 18. URL helper

- [ ] Detect `http://` and `https://` string values.
- [ ] Add a context menu action such as `Open URL in Browser`.
- [ ] Do not auto-open on single click (security risk).
- [ ] Show URL icon or underline in value cell.
- [ ] Heuristic: 10-digit or 13-digit integers in reasonable date range (1970-2100).
- [ ] Show a readable datetime in tooltip or secondary text (e.g., "2021-01-01 00:00:00 UTC").
- [ ] Keep raw value unchanged in the tree.
- [ ] Add context menu action: "Copy as ISO 8601".

### 18. URL helper

- [ ] Detect `http://` and `https://` string values.
- [ ] Add a context menu action such as `Open URL in Browser`.
- [ ] Do not auto-open on single click (security risk).
- [ ] Show URL icon or underline in value cell.

## Performance and Diagnostics

### 19. Performance metrics

- [ ] Log parse time, tree build time, render time in debug mode.
- [ ] Show load time in status bar for large files (e.g., "Loaded in 1.23s").
- [ ] Add debug mode flag to dump detailed performance stats to console.
- [ ] Track memory usage if practical (Windows: GetProcessMemoryInfo).

### 20. Progressive loading feedback

- [ ] Show progress bar for large file loading (>100MB).
- [ ] Show "Loading..." overlay during expensive operations.
- [ ] Allow cancellation of long-running operations (search, expand all).
- [ ] Show node count and depth stats in status bar (e.g., "1,234 nodes, depth 5").

## Threading Strategy

### 21. Threading policy

- [ ] Do not add threading as the first response to large files.
- [ ] First constrain work with lazy loading, chunking, and guarded actions.
- [ ] Add background work only for truly expensive bounded tasks.

### 22. Good threading candidates

- [ ] Expensive structured search over large currently-loaded scopes.
- [ ] Subtree prettify or export for medium-to-large bounded selections.
- [ ] Optional metadata or counting work (file statistics) that can be computed asynchronously.
- [ ] Background global search with streaming results.

### 23. Threaded global search

- [ ] Implement background search for large files.
- [ ] Run search in worker thread to avoid UI freeze.
- [ ] Stream results incrementally as they are found.
- [ ] Show progress indicator with "Found X matches so far...".
- [ ] Allow cancellation at any time.
- [ ] Limit total results to prevent memory explosion (e.g., max 10,000 matches).
- [ ] Provide "Search All Keys" and "Search All Values" options.

### 24. Bad threading candidates

- [ ] Using threads to hide unbounded `Expand All`.
- [ ] Using threads to mask full-tree materialization.
- [ ] Using threads instead of designing chunked loading for huge arrays or objects.

## Deferred / Not First Priority

### 25. Base64 helper

- [ ] Evaluate whether Base64 detection is common enough to justify UI complexity.
- [ ] If added later, prefer explicit actions like decode or save rather than automatic preview.

### 26. Color preview

- [ ] Consider previewing `#RRGGBB` or `rgb(...)` strings later.
- [ ] Treat this as a low-priority convenience feature.

### 27. Full JSONPath support

- [ ] Re-evaluate only after JSON Pointer jump and structured search are solid.
- [ ] Avoid early investment in a full JSONPath parser unless there is clear user demand.

### 28. Clipboard comparison

- [ ] Add context menu action: `Compare Node with Clipboard`.
- [ ] Parse clipboard content as JSON.
- [ ] Show side-by-side diff or highlight differences.
- [ ] Useful for debugging API responses.
- [ ] Defer until core features are stable.

## Never Implement (Anti-Goals)

- [ ] ❌ Full-document search with unlimited results (use bounded search instead).
- [ ] ❌ Export entire multi-GB file to other formats (CSV, Excel, YAML).
- [ ] ❌ In-place JSON editing (out of scope for preview plugin).
- [ ] ❌ JSON Schema validation (too expensive for large files).
- [ ] ❌ Diff/merge functionality (memory prohibitive).
- [ ] ❌ Automatic pretty-print of entire document (use Text View instead).
- [ ] ❌ Unbounded "Expand All" without warnings.

## Testing Strategy

### Test Files

- [ ] Small: <1MB, simple structure (e.g., config files).
- [ ] Medium: 10-50MB, nested objects (e.g., API responses).
- [ ] Large: 100-500MB, huge arrays (e.g., log files).
- [ ] Extreme: >1GB, deeply nested (e.g., database dumps).
- [ ] Malformed: invalid JSON, truncated files, encoding issues.
- [ ] Edge cases: empty arrays, null values, Unicode, escaped characters.

### Performance Benchmarks

- [ ] Parse time vs file size (target: <1s for 100MB).
- [ ] Memory usage vs file size (target: <2x file size).
- [ ] UI responsiveness during expansion (target: <100ms per expand).
- [ ] Search performance on large datasets (target: <5s for 100MB).

### Regression Tests

- [ ] Ensure lazy loading still works after optimizations.
- [ ] Verify pointer navigation accuracy.
- [ ] Test copy operations on various node types.
- [ ] Verify byte offset caching correctness.
- [ ] Test paging logic for large arrays.

## Acceptance Goals

- [ ] Medium and large JSON files should feel responsive during ordinary browsing.
- [ ] Common actions such as path copy, value copy, and incremental expansion should remain cheap.
- [ ] Expensive actions should be bounded, guarded, or explicitly rejected.
- [ ] The plugin should scale better because of `simdjson`, without pretending every full-document operation is cheap.
- [ ] Users should be able to explore multi-GB files without crashes or freezes.
- [ ] The plugin should provide clear feedback on file size, limitations, and progress.

