# JsonTreeViewer

JsonTreeViewer is a lightweight and efficient JSON viewer built using C++ and [simdjson](https://github.com/simdjson/simdjson/). It presents JSON data in a hierarchical tree structure, enabling users to navigate complex JSON files with ease.

## Features

- **Hierarchical Tree View**: Displays JSON data in an expandable and collapsible tree format for intuitive navigation.
- **Performance Optimized**: Utilizes simdjson for rapid parsing, ensuring swift and efficient performance.
- **User-Friendly Interface**: Offers an intuitive interface for seamless interaction with JSON data.

## Building and Running

To build and run JsonTreeViewer:

1. **Clone the Repository**

   ```bash
   git clone --recursive https://github.com/ccseer/JsonTreeViewer.git
   ```

2. **Open `JsonTreeViewer.pro`**
   - ```bash
        # TEMPLATE = lib
        # CONFIG += plugin
        # TARGET_EXT = .dll
        SOURCES += src/test.cpp
     ```
   - Build as exe
3. **Open the Project**

   Open JsonTreeViewer.pro with your preferred Qt development environment.

4. **Build and Run**

   Compile and run the project within the Qt environment.

## Seer Plugin

Developed as a DLL plugin for Seer 4.0.0

### TODO:

- expand path
- threading?
