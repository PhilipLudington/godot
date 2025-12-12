# GDScript Validation for Claude Code

## Quick Validation (Single Script)

**After editing a .gd file, validate it immediately:**

```bash
/Users/mrphil/Fun/godot-fork/bin/godot.macos.editor.arm64 \
  --headless --check-script /path/to/script.gd --output-format json
```

**Exit codes:**
- 0 = valid (no errors or warnings)
- 1 = errors found
- 2 = warnings only (no errors)

**JSON output example:**
```json
{"path":"/path/to/script.gd","valid":true,"errors":[],"warnings":[{"line":4,"column":5,"end_line":4,"end_column":29,"code":"UNUSED_VARIABLE","message":"..."}]}
```

**Text output (default):**
```
/path/to/script.gd:8:5: ERROR: Function "prnt()" not found in base self.
/path/to/script.gd:6:5: WARNING [UNUSED_VARIABLE]: The local variable "local_unused" is declared but never used.
```

---

## Full Project Validation

**Validate all .gd files in a directory:**

```bash
/Users/mrphil/Fun/godot-fork/bin/godot.macos.editor.arm64 \
  --headless --validate-scripts /path/to/project --output-format json
```

**JSON output example:**
```json
{"project_path":"/path/to/project","files_checked":42,"files_with_errors":3,"files_with_warnings":10,"total_errors":5,"total_warnings":15,"issues":[...]}
```

**Text output:**
```
/path/to/project/scripts/player.gd:3:5: ERROR: Function "prnt()" not found.
/path/to/project/scripts/utils.gd:10:5: WARNING [UNUSED_VARIABLE]: ...

Summary: 42 files checked, 5 errors, 15 warnings
```

---

## Autonomous Error-Fixing Workflow

When editing GDScript files, follow this loop:

```
1. Edit .gd file
2. Run validation command
3. If exit code != 0:
   a. Parse JSON output
   b. Fix errors first (severity: "error")
   c. Then fix warnings
   d. Go to step 2
4. Done when exit code == 0
```

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
      "column": 5,
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
- `UNUSED_VARIABLE` → Remove or use the variable, or prefix with `_`
- `UNUSED_PARAMETER` → Prefix parameter name with `_`
- `SHADOWED_VARIABLE` → Rename the variable
- `RETURN_VALUE_DISCARDED` → Assign result or use `@warning_ignore`
- Parse errors → Fix syntax at exact line:column

**After fixing, re-validate until exit code is 0.**

---

## Useful Commands

```bash
# Check issue count
jq '.metadata.total_issues' diagnostics/warnings.json

# Get hints
jq '.claude_hints' diagnostics/warnings.json

# List all issues with location
jq -r '.issues[] | "\(.file):\(.line):\(.column // 0) \(.severity): \(.message)"' diagnostics/warnings.json

# List only errors
jq -r '.issues[] | select(.severity == "error") | "\(.file):\(.line) - \(.message)"' diagnostics/warnings.json
```

---

## Planned Improvements

### --warnings-as-errors
Treat warnings as errors for stricter validation:
```bash
godot --headless --check-script /path/to/script.gd --warnings-as-errors
```
