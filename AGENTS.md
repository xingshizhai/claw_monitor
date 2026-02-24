# AGENTS.md - OpenClaw Monitor Project Guide

Essential information for AI agents working on the OpenClaw Monitor ESP32-S2 project.

## Project Overview

- **Type**: ESP-IDF (ESP32-S2) embedded C project
- **Purpose**: WiFi monitoring of OpenClaw Gateway with OLED display (SSD1306)
- **Main Component**: `main/openclaw_monitor_main.c`
- **Dependencies**: u8g2 graphics library (managed component)
- **Build System**: ESP-IDF CMake + Ninja
- **Target**: `esp32s2`
- **IDE Rules**: No Cursor rules (.cursorrules) or Copilot instructions (.github/copilot-instructions.md) found.

## Build Commands

```bash
# Set target (esp32s2)
idf.py set-target esp32s2

# Build the project
idf.py build

# Flash firmware to connected device
idf.py -p /dev/cu.usbserial-310 flash

# Flash and open serial monitor
idf.py -p /dev/cu.usbserial-310 flash monitor

# Monitor serial output without flashing
idf.py monitor

# Exit monitor: Ctrl+] (or Ctrl+T then Ctrl+X)

# Clean build (remove build artifacts)
idf.py clean

# Full clean rebuild
idf.py fullclean

# Open menuconfig for project configuration
idf.py menuconfig

# Flash specific parts
idf.py app-flash                 # Flash only app
idf.py bootloader-flash          # Flash only bootloader
idf.py partition-table-flash     # Flash only partition table

# Size analysis
idf.py size                      # Show memory usage
idf.py size-components           # Detailed memory usage
```

## Testing Commands

The project currently doesn't have unit tests. If tests are added later:

```bash
# Run all unit tests
idf.py test

# Run specific test component
idf.py test --component main

# Run single test case
idf.py test --test test_function_name
```

## Lint and Formatting

```bash
# Format all C/C++ files (if clang-format installed)
find . -name "*.c" -o -name "*.h" | xargs clang-format -i

# ESP-IDF linting
idf.py check                     # Check for common coding issues
idf.py analyze                   # Run static analysis
```

## Code Style Guidelines

### Naming Conventions
- **Variables**: `snake_case` (e.g., `display_dev_handle`)
- **Functions**: `snake_case` (e.g., `check_gateway_status`)
- **Macros**: `UPPER_CASE_WITH_UNDERSCORES` (e.g., `WIFI_MAX_RETRY`)
- **Types**: `snake_case_t` for typedefs (e.g., `monitor_state_t`)
- **Static functions**: `static` prefix with `snake_case`
- **Global variables**: `g_` prefix (e.g., `g_state`)
- **File-scope static variables**: no prefix (e.g., `i2c_bus_handle`)

### Import/Include Order
1. Standard C headers (`#include <stdio.h>`)
2. ESP-IDF system headers (`#include "freertos/FreeRTOS.h"`)
3. Project-specific headers (`#include "u8g2.h"`)
4. Local headers (relative paths)

### Error Handling
- Use `ESP_ERROR_CHECK()` for critical errors that should abort
- Use `esp_err_t` return types for functions that can fail
- Log errors with appropriate level: `ESP_LOGE`, `ESP_LOGW`, `ESP_LOGI`, `ESP_LOGD`
- Define a `TAG` at file scope: `static const char *TAG = "ComponentName";`

### Logging Guidelines
- Log at appropriate verbosity levels
- Use format strings compatible with `ESP_LOGI`
- Include relevant context (function names, error codes)
- Avoid logging in tight loops or interrupt handlers

### Memory Management
- Use `malloc()`/`free()` for general allocation
- For DMA-capable memory, use `heap_caps_malloc(size, MALLOC_CAP_DMA)`
- Always check `NULL` return from allocation functions
- Free allocated memory in error paths

### Formatting and Indentation
- **Indentation**: 4 spaces (no tabs)
- **Line length**: Aim for ≤ 100 characters
- **Braces**: K&R style (opening brace on same line)
- **Control statements**: Always use braces, even for single-line bodies
- **Pointer declarations**: `type *name` (star adjacent to type)

### Comments
- **Function comments**: Multi-line above function describing purpose, parameters, return value
- **Block comments**: `/* */` for multi-line explanations
- **Line comments**: `//` for single-line comments
- **TODO comments**: `// TODO: description` for future improvements
- **Avoid obvious comments**: Comment why, not what

### Types
- Use `stdint.h` types: `uint8_t`, `int32_t`, `uint64_t`, etc.
- Use `size_t` for sizes and indices
- Use `bool` from `stdbool.h` for boolean values
- Define structs with `typedef struct { ... } name_t;`
- Use enums for related constants


## Project Structure

```
openclaw-monitor/
├── main/                    # Main application component
│   ├── CMakeLists.txt      # Component build file
│   ├── idf_component.yml   # Component dependencies
│   ├── Kconfig.projbuild   # Project configuration
│   └── openclaw_monitor_main.c  # Main source
├── managed_components/      # External dependencies
│   └── u8g2/               # U8G2 graphics library
├── build/                  # Build artifacts (generated)
├── sdkconfig              # Current configuration
└── README.md              # Project documentation
```


*This file is maintained for AI agents working on the OpenClaw Monitor project. Update as the project evolves.*