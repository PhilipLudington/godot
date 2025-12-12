# Upstream File Modifications

This document tracks all files modified outside of `modules/warning_capture/` for the Warning Capture module. Use this to identify potential conflicts when rebasing onto new Godot versions.

## Overview

All changes are wrapped with `#ifdef WARNING_CAPTURE_ENABLED` guards, which is defined in `modules/warning_capture/SCsub` when this module is built.

## Modified Files

### modules/gdscript/gdscript.cpp

**Purpose:** Send GDScript compiler warnings to the editor debugger during editor-time compilation.

**Changes:**
- Added includes for `editor/editor_node.h` and `editor/debugger/editor_debugger_node.h` (guarded)
- Added code block in `GDScript::reload()` to call `EditorDebuggerNode::report_script_warning()` (guarded)

**Conflict Risk:** Medium - This file is actively developed upstream.

---

### editor/debugger/editor_debugger_node.cpp

**Purpose:** Expose debugger access and add script warning reporting.

**Changes:**
- Added `get_current_debugger()` and `get_default_debugger()` method bindings (guarded)
- Added `report_script_warning()` method implementation (guarded)
- Fixed signal PropertyInfo type hints (`Variant::OBJECT` instead of empty constructor) - marked with comment

**Conflict Risk:** Low - Method bindings section is stable.

---

### editor/debugger/editor_debugger_node.h

**Purpose:** Declare new API methods.

**Changes:**
- Added `report_script_warning()` declaration (guarded)

**Conflict Risk:** Low

---

### editor/debugger/script_editor_debugger.cpp

**Purpose:** Expose error/warning access API for external tools.

**Changes:**
- Added includes for `core/io/file_access.h` and `core/io/json.h` (guarded)
- Added method bindings for `get_all_errors()`, `dump_errors_to_file()`, `clear_errors()` (guarded)
- Fixed signal PropertyInfo type hints - marked with comment
- Added ~170 lines of new methods: `get_all_errors()`, `dump_errors_to_file()`, `clear_errors()`, `add_error_from_script()` (guarded)

**Conflict Risk:** Medium - Large addition, but at end of file.

---

### editor/debugger/script_editor_debugger.h

**Purpose:** Declare new API methods.

**Changes:**
- Added method declarations for error access API (guarded)

**Conflict Risk:** Low

---

### editor/editor_log.cpp

**Purpose:** Expose EditorLog API for programmatic access.

**Changes:**
- Added includes for `core/io/json.h` and `core/os/os.h` (guarded)
- Added `_bind_methods()` implementation (guarded)
- Added methods: `dump_messages_to_file()`, `get_all_messages()`, `get_error_count()`, `get_warning_count()` (guarded)

**Conflict Risk:** Low - New API additions at end of file.

---

### editor/editor_log.h

**Purpose:** Declare new API methods.

**Changes:**
- Added `_bind_methods()` declaration (guarded)
- Added API method declarations (guarded)

**Conflict Risk:** Low

---

### editor/editor_interface.cpp

**Purpose:** Expose EditorLog and EditorDebuggerNode access.

**Changes:**
- Added includes for debugger and log headers (guarded)
- Added `get_editor_log()` and `get_debugger_node()` methods (guarded)
- Added corresponding bind methods (guarded)

**Conflict Risk:** Low

---

### editor/editor_interface.h

**Purpose:** Declare accessor methods.

**Changes:**
- Added forward declarations for EditorDebuggerNode and EditorLog (guarded)
- Added method declarations (guarded)

**Conflict Risk:** Low

---

### editor/register_editor_types.cpp

**Purpose:** Register classes for GDScript access.

**Changes:**
- Added includes for debugger classes (guarded)
- Added GDREGISTER calls for EditorLog, EditorDebuggerNode, ScriptEditorDebugger (guarded)

**Conflict Risk:** Low - Registration section is additive.

---

## Signal Type Hint Fixes

The following signal type hint fixes are NOT guarded because they're bug fixes that should work with or without the module:

- `editor/debugger/editor_debugger_node.cpp`: Changed `PropertyInfo("script")` to `PropertyInfo(Variant::OBJECT, "script")`
- `editor/debugger/script_editor_debugger.cpp`: Same fix for breakpoint_selected, set_execution, clear_execution signals

These fix GDScript's ability to connect to these signals with proper type checking.

---

## Rebase Checklist

When rebasing onto a new Godot version:

1. [ ] Check `modules/gdscript/gdscript.cpp` for changes to `reload()` method
2. [ ] Check `editor/debugger/` for API changes
3. [ ] Check `editor/editor_interface.cpp` for new accessor patterns
4. [ ] Check `editor/register_editor_types.cpp` for registration pattern changes
5. [ ] Run a full build with the module enabled
6. [ ] Run a build with `module_warning_capture_enabled=no` to verify guards work

## Version History

| Godot Version | Last Rebased | Notes |
|---------------|--------------|-------|
| 4.4.1-stable  | 2025-11-15   | Initial implementation |
