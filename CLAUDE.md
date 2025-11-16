# Instructions for Claude Code

This document provides instructions for Claude Code (AI assistant) to validate and fix GDScript code autonomously.

---

## How to Validate GDScript Files

When you need to check for GDScript errors and warnings, follow these steps:

### Step 1: Run Validation

Execute this command from the project root:

```bash
/Users/mrphil/Fun/godot-fork/bin/godot.macos.editor.arm64 \
  --headless \
  --path . \
  --quit \
  -e \
  --script res://path/to/validate.gd
```

Or use the simpler approach - create a temporary validation script:

```bash
# Create validation script
cat > /tmp/validate_gdscript.gd << 'EOF'
extends SceneTree

func _init():
    print("Running GDScript validation...")
    WarningCapture.capture_debugger_warnings_now()

    var file = FileAccess.open("res://diagnostics/warnings.json", FileAccess.READ)
    if !file:
        print("ERROR: Could not read diagnostics")
        quit(1)
        return

    var json = JSON.new()
    json.parse(file.get_as_text())
    var data = json.get_data()
    file.close()

    var total = data.get("metadata", {}).get("total_issues", 0)
    var errors = data.get("metadata", {}).get("by_severity", {}).get("error", 0)

    print("Total issues: %d (Errors: %d)" % [total, errors])

    if errors > 0:
        quit(1)
    else:
        quit(0)
EOF

# Run validation
/Users/mrphil/Fun/godot-fork/bin/godot.macos.editor.arm64 \
  --headless \
  --path . \
  --script /tmp/validate_gdscript.gd \
  --quit

# Capture exit code
VALIDATION_EXIT_CODE=$?
```

### Step 2: Check Exit Code

- **Exit code 0**: Validation passed (no errors)
- **Exit code 1**: Validation failed (errors found)

```bash
if [ $VALIDATION_EXIT_CODE -eq 0 ]; then
    echo "✅ Validation passed"
else
    echo "❌ Validation failed - check diagnostics/warnings.json"
fi
```

### Step 3: Read Diagnostics

Read the validation results:

```bash
cat diagnostics/warnings.json
```

The JSON structure:

```json
{
  "metadata": {
    "total_issues": 15,
    "by_severity": {
      "error": 2,
      "warning": 13
    },
    "by_category": {
      "unused_variable": 10,
      "shadowed_variable": 3,
      "parse_error": 2
    }
  },
  "claude_hints": {
    "start_with_file": "res://scripts/player.gd",
    "max_issues_in_file": 5,
    "batch_fixes_available": ["unused_variable"],
    "estimated_fix_time": "5-15 minutes",
    "priority_order": "errors_first"
  },
  "issues": [
    {
      "file": "res://scripts/player.gd",
      "line": 45,
      "message": "The variable 'old_position' is declared but never used",
      "code": "UNUSED_VARIABLE",
      "severity": "warning",
      "category": "unused_variable"
    }
  ]
}
```

### Step 4: Analyze Issues

Use `claude_hints` to determine fixing strategy:

1. **priority_order**: Always "errors_first" - fix errors before warnings
2. **start_with_file**: File with the most issues - start here
3. **batch_fixes_available**: Categories that can be fixed in bulk (e.g., all unused variables)
4. **estimated_fix_time**: How long fixing should take

### Step 5: Fix Issues

For each issue in the `issues` array:

1. Open the file: `issue.file`
2. Go to line: `issue.line`
3. Read the error message: `issue.message`
4. Apply the appropriate fix based on `issue.code`:

**Common fix patterns:**

- `UNUSED_VARIABLE`: Remove the variable declaration
- `SHADOWED_VARIABLE`: Rename the variable to avoid shadowing
- `Parse Error`: Fix syntax error at the specified line
- `RETURN_VALUE_DISCARDED`: Add `_` prefix or assign to variable

### Step 6: Re-validate

After making fixes, run validation again (Step 1).

Repeat Steps 1-6 until validation passes (exit code 0).

---

## Complete Workflow Example

```bash
# 1. Make changes to GDScript files
# (You edit the files here)

# 2. Validate
cat > /tmp/validate.gd << 'EOF'
extends SceneTree
func _init():
    WarningCapture.capture_debugger_warnings_now()
    var file = FileAccess.open("res://diagnostics/warnings.json", FileAccess.READ)
    var json = JSON.new()
    json.parse(file.get_as_text())
    var data = json.get_data()
    file.close()
    var errors = data.get("metadata", {}).get("by_severity", {}).get("error", 0)
    quit(1 if errors > 0 else 0)
EOF

/Users/mrphil/Fun/godot-fork/bin/godot.macos.editor.arm64 \
  --headless --path . --script /tmp/validate.gd --quit

# 3. Check results
if [ $? -eq 0 ]; then
    echo "✅ Clean"
else
    # 4. Read and analyze
    cat diagnostics/warnings.json | jq '.claude_hints'

    # 5. Fix issues
    # (Apply fixes based on issues array)

    # 6. Re-validate
    # (Run validation again)
fi
```

---

## Quick Commands

### One-liner validation:

```bash
/Users/mrphil/Fun/godot-fork/bin/godot.macos.editor.arm64 --headless --path . --script <(echo 'extends SceneTree
func _init():
    WarningCapture.capture_debugger_warnings_now()
    var file = FileAccess.open("res://diagnostics/warnings.json", FileAccess.READ)
    var json = JSON.new()
    json.parse(file.get_as_text())
    var data = json.get_data()
    file.close()
    quit(1 if data.get("metadata", {}).get("by_severity", {}).get("error", 0) > 0 else 0)') --quit
```

### Check issue count:

```bash
cat diagnostics/warnings.json | jq '.metadata.total_issues'
```

### Get Claude hints:

```bash
cat diagnostics/warnings.json | jq '.claude_hints'
```

### List all issues:

```bash
cat diagnostics/warnings.json | jq '.issues[] | "\(.file):\(.line) - \(.message)"'
```

---

## Important Notes

### Exit Codes
- **0** = Validation passed (no errors, warnings OK)
- **1** = Validation failed (errors found)

Use exit codes to control your workflow:
```bash
if validation_passes; then
    continue_with_next_step
else
    fix_issues_and_retry
fi
```

### Validation Speed
- Small projects (<50 files): 1-5 seconds
- Medium projects (50-200 files): 5-15 seconds
- Large projects (200-500 files): 15-45 seconds

### File Locations
- **Input**: All `*.gd` files in the project
- **Output**: `diagnostics/warnings.json`
- **Metadata**: `diagnostics/.scan_metadata.json` (performance info)

### Validation Scope
- ✅ Compile-time warnings (unused variables, shadowed variables, etc.)
- ✅ Parse errors (syntax errors)
- ✅ Type errors
- ❌ Runtime errors (only in GUI mode when game runs)

---

## Troubleshooting

### "Could not read diagnostics/warnings.json"

**Cause**: Validation hasn't run yet or module not initialized

**Solution**:
1. Check that Godot binary includes the warning_capture module
2. Ensure you're running from the project root
3. Try running in GUI mode first to verify module works

### "Validation takes too long"

**Cause**: Large project or complex files

**Solution**: Check timeout settings in project.godot:
```
[diagnostics]
total_scan_timeout_ms = 90000
per_file_timeout_ms = 500
```

### "Missing issues I can see in the editor"

**Cause**:
- Runtime errors (only captured when game runs)
- Editor-only warnings

**Solution**: Headless validation only captures compile-time issues. For runtime errors, the game must be executed.

---

## Summary

**To validate GDScript code:**

1. ✅ Run Godot headless with validation script
2. ✅ Check exit code (0 = success, 1 = errors)
3. ✅ Read `diagnostics/warnings.json`
4. ✅ Use `claude_hints` for fixing strategy
5. ✅ Fix issues from the `issues` array
6. ✅ Re-validate until exit code is 0

**Simple validation command:**

```bash
/Users/mrphil/Fun/godot-fork/bin/godot.macos.editor.arm64 \
  --headless --path . \
  --script /tmp/validate.gd --quit \
  && echo "✅ Clean" \
  || echo "❌ Issues found - check diagnostics/warnings.json"
```

**No external scripts needed - just use the Godot binary and read the JSON!**
