# GDScript Compiler Warning Flow Analysis

## Executive Summary

This document traces how GDScript compiler warnings flow through the Godot editor's logging system and identifies where file/line information gets lost.

## Warning Generation Flow

### 1. GDScript Warning Generation (modules/gdscript/)

**File: `modules/gdscript/gdscript_warning.h`**
- Defines `GDScriptWarning` class with warning codes (e.g., `INCOMPATIBLE_TERNARY`)
- Stores metadata: `start_line`, `end_line`, `leftmost_column`, `rightmost_column`
- Contains `symbols` vector for contextual information

**File: `modules/gdscript/gdscript_analyzer.cpp`**
- Generates warnings during analysis phase
- Example: `parser->push_warning(p_ternary_op, GDScriptWarning::INCOMPATIBLE_TERNARY);`
- Calls `push_warning()` which stores pending warnings

**File: `modules/gdscript/gdscript_parser.cpp`**

Structure: `PendingWarning` (line 1351-1356)
```cpp
struct PendingWarning {
    const Node *source = nullptr;  // Pointer to AST node (has line info)
    GDScriptWarning::Code code;
    bool treated_as_error = false;
    Vector<String> symbols;
};
```

Key function: `push_warning()` (line 192-214)
```cpp
void GDScriptParser::push_warning(const Node *p_source, 
                                 GDScriptWarning::Code p_code, 
                                 const Vector<String> &p_symbols) {
    // ... validation ...
    PendingWarning pw;
    pw.source = p_source;
    pw.code = p_code;
    pw.symbols = p_symbols;
    pending_warnings.push_back(pw);
}
```

Key function: `apply_pending_warnings()` (line 216-253)
```cpp
void GDScriptParser::apply_pending_warnings() {
    for (const PendingWarning &pw : pending_warnings) {
        // ... validation ...
        GDScriptWarning warning;
        warning.code = pw.code;
        warning.symbols = pw.symbols;
        warning.start_line = pw.source->start_line;      // LINE INFO EXTRACTED
        warning.end_line = pw.source->end_line;
        warning.leftmost_column = pw.source->leftmost_column;
        warning.rightmost_column = pw.source->rightmost_column;
        
        // ... add to warnings list ...
    }
    pending_warnings.clear();
}
```

**Access point: `gdscript_parser.h` line 1600**
```cpp
const List<GDScriptWarning> &get_warnings() const { return warnings; }
```

## 2. Warning Transmission to Editor Debugger

### Files: `modules/gdscript/gdscript.cpp`

Location: Line 876-882
```cpp
#ifdef DEBUG_ENABLED
for (const GDScriptWarning &warning : parser.get_warnings()) {
    if (EngineDebugger::is_active()) {
        Vector<ScriptLanguage::StackInfo> si;
        EngineDebugger::get_script_debugger()->send_error(
            "",                      // p_func (empty for warnings)
            get_script_path(),        // p_file (CONTAINS FILE PATH)
            warning.start_line,       // p_line (CONTAINS LINE NUMBER)
            warning.get_name(),       // p_err (warning code name)
            warning.get_message(),    // p_descr (warning message)
            false,                    // p_editor_notify
            ERR_HANDLER_WARNING,      // p_type
            si                        // p_stack_info
        );
    }
}
#endif
```

**KEY FINDING**: The file path and line number ARE being passed to `send_error()`:
- `p_file` = `get_script_path()` (full file path)
- `p_line` = `warning.start_line` (line number)

## 3. Debugger Message Processing

### Files: `core/debugger/script_debugger.h` and `core/debugger/script_debugger.cpp`

Function: `send_error()` (line 89-94 in script_debugger.cpp)
```cpp
void ScriptDebugger::send_error(const String &p_func, 
                                const String &p_file, 
                                int p_line,
                                const String &p_err, 
                                const String &p_descr,
                                bool p_editor_notify, 
                                ErrorHandlerType p_type, 
                                const Vector<StackInfo> &p_stack_info) {
    error_stack_info.append_array(p_stack_info);
    EngineDebugger::get_singleton()->send_error(
        p_func, p_file, p_line, p_err, p_descr, p_editor_notify, p_type
    );
    error_stack_info.clear();
}
```

**KEY FINDING**: File and line info passed through unchanged.

### Files: `core/debugger/remote_debugger.cpp`

Function: `send_error()` (line 272-324)
```cpp
void RemoteDebugger::send_error(const String &p_func, 
                                const String &p_file, 
                                int p_line,
                                const String &p_err, 
                                const String &p_descr,
                                bool p_editor_notify, 
                                ErrorHandlerType p_type) {
    ErrorMessage oe;
    oe.error = p_err;
    oe.error_descr = p_descr;
    oe.source_file = p_file;      // FILE STORED HERE
    oe.source_line = p_line;       // LINE STORED HERE
    oe.source_func = p_func;
    oe.warning = p_type == ERR_HANDLER_WARNING;
    // ... time tracking ...
    oe.callstack.append_array(script_debugger->get_error_stack_info());
    // ... add to errors queue ...
}
```

**KEY FINDING**: File and line info stored in `ErrorMessage` struct in `oe.source_file` and `oe.source_line`.

## 4. Editor-Side Warning Display

### Files: `editor/debugger/script_editor_debugger.cpp`

Function: `_parse_message()` (line 549-670, current version with patch)
```cpp
// Line 549-552
bool source_is_project_file = oe.source_file.begins_with("res://");

// Line 553-556 - ALWAYS CREATE METADATA (after patch)
Array source_meta;
source_meta.push_back(oe.source_file);    // FILE PATH
source_meta.push_back(oe.source_line);    // LINE NUMBER

// Line 621-623 - ALWAYS SET METADATA (after patch)
error->set_metadata(0, source_meta);
```

**KEY FINDING**: The patch from the current branch ALWAYS sets metadata now (not just for project files).

## 5. EditorLog Integration

### Files: `editor/editor_log.cpp`

The `EditorLog::dump_messages_to_file()` and `get_all_messages()` methods extract messages:
```cpp
// Line 569-593
for (const LogMessage &msg : messages) {
    Dictionary d;
    // ... populate with type, text, count ...
    String json_line = JSON::stringify(d);
    f->store_line(json_line);
}
```

**KEY FINDING**: The current `EditorLog` implementation does NOT have access to the file/line metadata because:
1. Warnings go through the debugger's error tree
2. EditorLog receives only text-formatted messages
3. No structured metadata is passed to EditorLog

## 6. The Missing Link: File/Line Information Loss

### Problem Identification

The issue occurs because compiler warnings follow TWO DIFFERENT PATHS:

**Path 1: Editor Error/Warning Display (Debugger Error Tree)**
- Compiler warning → ScriptDebugger → RemoteDebugger → ScriptEditorDebugger → Error Tree
- **Has file/line info**: Stored in `oe.source_file` and `oe.source_line` in `_parse_message()`
- **Result**: File/line info properly displayed in the error tree UI

**Path 2: EditorLog Integration (if warnings also logged here)**
- Compiler warning → ??? → EditorLog
- **Missing file/line**: EditorLog receives only the formatted message text
- **Result**: Empty file and 0 line in logged output

### Root Cause Analysis

**1. For warnings coming through ScriptEditorDebugger API:**
The `get_all_errors()` method in the patch correctly extracts metadata:
```cpp
Variant meta_value = item->get_metadata(0);
if (meta_value.get_type() == Variant::ARRAY) {
    Array meta = meta_value;
    if (meta.size() >= 2) {
        error_dict["source_file"] = meta[0];  // Should have file path
        error_dict["source_line"] = meta[1];  // Should have line number
    }
}
```

BUT if metadata isn't set at the time the tree item is created, these will be empty!

**2. For warnings that might come through EditorLog:**
EditorLog has no mechanism to receive file/line metadata at all.

## Critical Code Points

### Where File/Line Information IS Available:
1. **gdscript.cpp:880** - `send_error()` call with `get_script_path()` and `warning.start_line`
2. **remote_debugger.cpp:276-277** - `oe.source_file` and `oe.source_line` assignment
3. **script_editor_debugger.cpp:553-556** - `source_meta` array creation with file/line

### Where File/Line Information MUST be Preserved:
1. **script_editor_debugger.cpp** - When creating error tree items (already fixed in patch)
2. **editor_log.cpp** - If warnings are also sent here, needs structured data

## Data Flow Diagram

```
GDScript Analysis
├─ Warning Code + Symbols
└─ Node* (has start_line, end_line)
    │
    └─→ push_warning()
        └─→ PendingWarning(source, code)
            │
            └─→ apply_pending_warnings()
                └─→ GDScriptWarning (populated with line info)
                    │
                    └─→ parser.warnings (List<GDScriptWarning>)
                        │
                        └─→ gdscript.cpp:880 send_error()
                            ├─ String p_file (SCRIPT PATH)
                            └─ int p_line (LINE NUMBER)
                                │
                                └─→ ScriptDebugger::send_error()
                                    │
                                    └─→ RemoteDebugger::send_error()
                                        └─→ ErrorMessage oe
                                            ├─ oe.source_file
                                            └─ oe.source_line
                                                │
                                                └─→ ScriptEditorDebugger::_parse_message()
                                                    └─→ Array source_meta
                                                        ├─ [0] = file path
                                                        └─ [1] = line number
                                                            │
                                                            └─→ error_tree_item->set_metadata(0, source_meta)
                                                                │
                                                                └─→ ScriptEditorDebugger::get_all_errors()
                                                                    └─→ item->get_metadata(0)
                                                                        ├─ error_dict["source_file"]
                                                                        └─ error_dict["source_line"]
```

## Summary

**The file/line information IS present throughout the entire flow** from compiler warning generation through to the debugger's error tree display.

The issue appears to be:
1. **If metadata is empty**: The tree item metadata wasn't set when the warning was first added
2. **If using EditorLog directly**: EditorLog API doesn't receive structured metadata, only text messages

The patch in the current branch addresses #1 by always setting metadata for all messages.

For #2, if warnings are also being logged through EditorLog, that system would need enhancement to pass structured data instead of just formatted text strings.
