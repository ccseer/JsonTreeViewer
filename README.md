# JsonTreeViewer

<!-- ## Screenshots

![](res/screenshot.png) -->

JsonTreeViewer is a JSON tree viewer built with C++ and powered by [simdjson](https://github.com/simdjson/simdjson/) 4.6.3. This repository uses simdjson as its high-performance parsing engine and targets fast browsing of large and very large JSON files.

## Building and Running

Requirements: Qt 6.8, CMake 3.16+.

1. **Clone the Repository**

   ```bash
   git clone https://github.com/ccseer/JsonTreeViewer.git
   cd JsonTreeViewer
   ```

2. **Build**

   ```bash
   cmake -B build
   cmake --build build
   ```

   This produces two outputs:
   - `jsontreeviewer.dll` — the Seer plugin
   - `test_jsontreeviewer.exe` — standalone viewer for testing

3. **Install the plugin**

   Copy `jsontreeviewer.dll` to your Seer plugins directory.

## Seer Plugin

JsonTreeViewer is a file preview plugin for [Seer](https://1218.io) — a quick-look tool for Windows.

- Displays JSON data in an expandable hierarchical tree structure
- Uses [simdjson](https://github.com/simdjson/simdjson/) 4.6.3 for high-performance JSON parsing
- Intended for viewing large and very large JSON files inside Seer
- Supports filtering and searching within JSON data
- Built as a native DLL plugin for Seer 4.0.0+

Visit [1218.io](https://1218.io) to download Seer and learn more about the plugin ecosystem.
