# GDScript Validation for Claude Code

## Quick Start

**Validate GDScript files:**

```bash
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
```

**Exit codes:** 0 = success, 1 = errors found

---

## Read Results

```bash
cat diagnostics/warnings.json
```

**JSON structure:**

```json
{
  "claude_hints": {
    "start_with_file": "res://scripts/player.gd",
    "batch_fixes_available": ["unused_variable"],
    "priority_order": "errors_first"
  },
  "issues": [
    {
      "file": "res://scripts/player.gd",
      "line": 45,
      "message": "The variable 'x' is declared but never used",
      "code": "UNUSED_VARIABLE",
      "severity": "warning"
    }
  ]
}
```

---

## Fix Issues

**Strategy:**
1. Fix errors first (severity: "error")
2. Use `claude_hints.start_with_file` for file priority
3. Batch fix categories in `claude_hints.batch_fixes_available`

**Common fixes:**
- `UNUSED_VARIABLE` → Remove the variable
- `SHADOWED_VARIABLE` → Rename the variable
- Parse errors → Fix syntax at line number

**After fixing, re-validate until exit code is 0.**

---

## Useful Commands

```bash
# Check issue count
jq '.metadata.total_issues' diagnostics/warnings.json

# Get hints
jq '.claude_hints' diagnostics/warnings.json

# List all issues
jq -r '.issues[] | "\(.file):\(.line) - \(.message)"' diagnostics/warnings.json
```
