# Warning Capture Plugin - Usage Guide

## Problem: EditorDebuggerNode Not Exposed to GDScript

The main issue is that `EditorDebuggerNode` and its methods are not exposed to GDScript, which prevents direct access to debugger warnings from plugins.

## Solution Options

### Option 1: Use the C++ Module (Recommended)

I've created a C++ module that properly exposes the warning capture functionality to GDScript.

#### Building with the Module

```bash
# From the godot-fork directory
scons platform=macos arch=arm64 vulkan=no module_warning_capture_enabled=yes -j8
```

#### Using the Module in Your Plugin

```gdscript
@tool
extends EditorPlugin

var warning_capture: RefCounted

func _enter_tree():
    if ClassDB.class_exists("WarningCaptureEditor"):
        warning_capture = ClassDB.instantiate("WarningCaptureEditor")

        # Get all warnings with file/line info
        var errors = warning_capture.get_all_warnings_and_errors()
        for error in errors:
            print("File: ", error.source_file)
            print("Line: ", error.source_line)
            print("Message: ", error.message)

        # Save to file
        warning_capture.dump_errors_to_file("res://warnings.json")
```

### Option 2: Manual Verification (Current Workaround)

Since the C++ fix is working but GDScript can't access it directly:

1. **Open Godot Editor** with your test project
2. **Open the Debugger Panel** (bottom panel)
3. **Click on the Errors tab**
4. **Open a script with warnings** (like `simple_warning_test.gd`)
5. **Verify warnings show with file/line info**

The warnings should now display as:
- `res://simple_warning_test.gd:7` instead of empty file and line 0

### Option 3: External Tool Integration

Create an external Python/Node.js script that:
1. Launches Godot with specific flags
2. Parses the output or connects to the debugger
3. Extracts warning information
4. Saves to a file for processing

## What's Working Now

✅ **C++ Layer**: Warnings are properly captured with file/line information
✅ **Debugger Display**: The Errors panel shows correct file/line info
✅ **API Methods**: `get_all_errors()` returns proper metadata (accessible from C++)

## What's Not Working (Yet)

❌ **GDScript Access**: Can't directly call debugger methods from GDScript plugins
❌ **Automated Capture**: Plugins can't automatically extract warnings without C++ module

## Files Provided

### For Testing (in test_project/):
- `addons/warning_capture/plugin.gd` - Basic plugin showing the limitation
- `addons/warning_capture/plugin_with_module.gd` - Plugin that uses the C++ module
- `simple_warning_test.gd` - Test script with warnings

### For C++ Module (in modules/warning_capture/):
- `config.py` - Module configuration
- `SCsub` - Build configuration
- `register_types.cpp/h` - Module registration
- `editor/warning_capture_editor.cpp/h` - Actual implementation

## Next Steps

### To Use Immediately:
1. Build Godot with the warning_capture module
2. Use `plugin_with_module.gd` in your project
3. Access warnings programmatically

### For Official Support:
Consider submitting a PR to Godot to:
1. Expose EditorDebuggerNode to GDScript
2. Or add official warning capture API methods to EditorInterface

## Example Output

When working correctly, the plugin will save a JSON file like:
```json
{
    "time": "0:00:05:123",
    "message": "Values of the ternary operator are not mutually compatible.",
    "is_warning": true,
    "source_file": "res://simple_warning_test.gd",
    "source_line": 7,
    "error_condition": "INCOMPATIBLE_TERNARY"
}
```

This file can then be processed by external tools like Claude Code to automatically fix the warnings.