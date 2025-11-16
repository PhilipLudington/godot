# Claude Code + Godot Headless: Zero-Intervention Workflow

**Goal:** Claude Code makes changes → validates with Godot headless → reads diagnostics → fixes issues → repeats until clean.

---

## The Perfect Workflow

### **Step-by-Step Process**

```
1. Claude Code edits GDScript files
2. Claude Code runs: godot --headless --script validate.gd --quit
3. Godot validates all scripts → writes diagnostics/warnings.json
4. Claude Code reads warnings.json
5. If issues found:
   - Claude Code analyzes claude_hints
   - Claude Code fixes issues
   - Go to step 2 (re-validate)
6. If clean:
   - Done! ✅
```

### **Key Insight**

You don't need Godot running in the background. Just run it **each time Claude Code is ready to validate**:
- After editing files
- Before committing
- During iterative fixing

---

## Setup (One-Time)

### 1. Create Validation Script

Create `validate_gdscript.gd` in your project root:

```gdscript
# validate_gdscript.gd
extends SceneTree

func _init():
	print("=" * 60)
	print("GODOT GDSCRIPT VALIDATION")
	print("=" * 60)

	# Run validation
	var start_time = Time.get_ticks_msec()
	WarningCapture.capture_debugger_warnings_now()
	var duration = Time.get_ticks_msec() - start_time

	# Read results
	var file = FileAccess.open("res://diagnostics/warnings.json", FileAccess.READ)
	if !file:
		print("ERROR: Could not read diagnostics/warnings.json")
		quit(1)
		return

	var json_text = file.get_as_text()
	file.close()

	var json = JSON.new()
	var parse_result = json.parse(json_text)
	if parse_result != OK:
		print("ERROR: Could not parse JSON")
		quit(1)
		return

	var data = json.get_data()

	# Print summary
	print("\nValidation completed in %d ms" % duration)
	print("-" * 60)

	var metadata = data.get("metadata", {})
	var total = metadata.get("total_issues", 0)
	var by_severity = metadata.get("by_severity", {})
	var errors = by_severity.get("error", 0)
	var warnings = by_severity.get("warning", 0)

	print("Total issues: %d" % total)
	print("  - Errors:   %d" % errors)
	print("  - Warnings: %d" % warnings)

	if total > 0:
		print("\nClaude Code hints:")
		var hints = data.get("claude_hints", {})
		print("  - Start with: %s" % hints.get("start_with_file", "N/A"))
		print("  - Batch fixable: %s" % hints.get("batch_fixes_available", []))
		print("  - Estimated time: %s" % hints.get("estimated_fix_time", "N/A"))

	print("=" * 60)
	print("Diagnostics written to: diagnostics/warnings.json")
	print("=" * 60)

	# Exit with error code if issues found
	if errors > 0:
		print("\n❌ VALIDATION FAILED - Errors found")
		quit(1)
	elif warnings > 0:
		print("\n⚠️  VALIDATION PASSED - Warnings found")
		quit(0)  # or quit(1) if you want warnings to fail
	else:
		print("\n✅ VALIDATION PASSED - No issues")
		quit(0)
```

### 2. Create Shell Wrapper (Optional)

Create `validate.sh` in your project root:

```bash
#!/bin/bash
# validate.sh - Convenient wrapper for Claude Code

GODOT_BIN="${GODOT_BIN:-/Users/mrphil/Fun/godot-fork/bin/godot.macos.editor.arm64}"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "Running Godot validation..."
"$GODOT_BIN" --headless --path "$PROJECT_DIR" --script validate_gdscript.gd --quit

EXIT_CODE=$?

if [ $EXIT_CODE -eq 0 ]; then
    echo ""
    echo "✅ Ready for Claude Code to proceed"
else
    echo ""
    echo "❌ Issues found - Claude Code should read diagnostics/warnings.json"
fi

exit $EXIT_CODE
```

```bash
chmod +x validate.sh
```

---

## Claude Code Integration

### **Method 1: Manual Command (Simplest)**

**You tell Claude Code:**

```
I've made changes to the GDScript files. Please:
1. Run: ./validate.sh
2. Read diagnostics/warnings.json
3. Fix any issues found
4. Repeat until clean
```

**Claude Code does:**

```bash
# Run validation
./validate.sh

# Read results
cat diagnostics/warnings.json

# Analyze and fix...
```

### **Method 2: Automated Tool (Recommended)**

Give Claude Code this tool/function:

```javascript
// For Claude Code MCP or tool integration
async function validateGodotScripts(projectPath) {
    const { execSync } = require('child_process');
    const fs = require('fs');
    const path = require('path');

    // Run validation
    try {
        execSync('./validate.sh', {
            cwd: projectPath,
            stdio: 'inherit'
        });
    } catch (error) {
        // Non-zero exit code means issues found (expected)
    }

    // Read diagnostics
    const diagPath = path.join(projectPath, 'diagnostics', 'warnings.json');
    const diagnostics = JSON.parse(fs.readFileSync(diagPath, 'utf8'));

    return {
        total_issues: diagnostics.metadata.total_issues,
        errors: diagnostics.metadata.by_severity.error || 0,
        warnings: diagnostics.metadata.by_severity.warning || 0,
        claude_hints: diagnostics.claude_hints,
        issues: diagnostics.issues
    };
}
```

**Usage in Claude Code:**

```
After making changes:
1. Call validateGodotScripts()
2. If issues.length > 0: analyze and fix
3. Repeat
```

### **Method 3: Git Pre-Commit Hook**

Automatically validate before commits:

```bash
#!/bin/bash
# .git/hooks/pre-commit

echo "Running GDScript validation..."

./validate.sh

if [ $? -ne 0 ]; then
    echo ""
    echo "❌ Commit blocked - GDScript validation failed"
    echo "Fix issues or use 'git commit --no-verify' to skip"
    exit 1
fi

echo "✅ Validation passed - proceeding with commit"
```

---

## Complete Claude Code Workflow Example

### **Scenario: Claude Code fixes all warnings autonomously**

**Initial State:**
```
user: "Fix all GDScript warnings in the project"
```

**Claude Code Iteration 1:**

```bash
# 1. Run validation
./validate.sh

# Output:
# Total issues: 15
#   - Errors: 0
#   - Warnings: 15
# Claude Code hints:
#   - Start with: res://scripts/player.gd
#   - Batch fixable: ["unused_variable"]
#   - Estimated time: 5-15 minutes
```

```javascript
// 2. Read diagnostics
const diag = JSON.parse(fs.readFileSync('diagnostics/warnings.json'));

// 3. Analyze
// - 15 unused variables across 8 files
// - Batch fixable category detected
// - Start with player.gd (5 issues)
```

**Claude Code actions:**
```
// 4. Fix batch issues
- Remove all 15 unused variable declarations
- Files modified: 8 files
```

**Claude Code Iteration 2:**

```bash
# 5. Re-validate
./validate.sh

# Output:
# ✅ VALIDATION PASSED - No issues
```

**Result:**
```
✅ All warnings fixed in 1 iteration
✅ Zero human intervention
✅ Ready to commit
```

---

## Advanced: Iterative Fix-and-Validate Loop

### **Smart Claude Code Script**

```javascript
// auto_fix_gdscript.js
const { execSync } = require('child_process');
const fs = require('fs');

const MAX_ITERATIONS = 10;
const PROJECT_PATH = process.cwd();

async function validateAndFix() {
    for (let iteration = 1; iteration <= MAX_ITERATIONS; iteration++) {
        console.log(`\n🔄 Iteration ${iteration}/${MAX_ITERATIONS}`);

        // Run validation
        let exitCode = 0;
        try {
            execSync('./validate.sh', {
                cwd: PROJECT_PATH,
                stdio: 'inherit'
            });
        } catch (error) {
            exitCode = error.status;
        }

        // Read diagnostics
        const diagPath = `${PROJECT_PATH}/diagnostics/warnings.json`;
        const diag = JSON.parse(fs.readFileSync(diagPath, 'utf8'));

        const total = diag.metadata.total_issues;
        const errors = diag.metadata.by_severity.error || 0;

        if (total === 0) {
            console.log('\n✅ All issues resolved!');
            return true;
        }

        console.log(`📊 Found ${total} issues (${errors} errors)`);
        console.log(`💡 Claude hints: ${JSON.stringify(diag.claude_hints, null, 2)}`);

        // TODO: Call Claude API to fix issues
        // await claudeFixIssues(diag);

        console.log('⏳ Waiting for Claude to apply fixes...');
        // In practice, Claude Code would make edits here

        if (iteration === MAX_ITERATIONS) {
            console.log('\n⚠️  Max iterations reached - manual intervention needed');
            return false;
        }
    }
}

validateAndFix();
```

---

## Performance Considerations

### **Validation Speed**

Typical validation times:
- **Small project** (<50 files): 1-3 seconds
- **Medium project** (50-200 files): 5-15 seconds
- **Large project** (200-500 files): 15-45 seconds
- **Very large** (500+ files): 45-90 seconds

### **Optimization Tips**

1. **Only validate changed files** (future enhancement):
   ```bash
   # Get changed files
   git diff --name-only HEAD | grep '\.gd$' > changed_files.txt

   # Pass to validator (would need code changes)
   ./validate.sh --only-files changed_files.txt
   ```

2. **Parallel validation** (future enhancement):
   - Split files into batches
   - Run multiple Godot instances
   - Merge results

3. **Cache results**:
   - Store file hash → validation result
   - Skip unchanged files
   - Much faster on iteration

---

## Troubleshooting

### **Problem: Validation takes too long**

**Solution 1:** Increase timeout in project settings
```gdscript
# In project.godot or via script
ProjectSettings.set_setting("diagnostics/total_scan_timeout_ms", 180000)  # 3 minutes
```

**Solution 2:** Validate only changed files
```bash
# Only validate files modified in last commit
git diff --name-only HEAD~1 HEAD | grep '\.gd$' > validate_these.txt
```

### **Problem: Exit code always 0 even with errors**

**Check:** Make sure `validate_gdscript.gd` calls `quit(1)` for errors:
```gdscript
if errors > 0:
    quit(1)  # Non-zero exit code
else:
    quit(0)  # Zero exit code
```

### **Problem: JSON file not created**

**Check:**
1. Module initialized? Look for "WARNING CAPTURE MODULE: Initialized"
2. Permissions on diagnostics/ folder?
3. Run in non-headless mode first to verify module works

---

## Summary: The Perfect Claude Code Loop

```
┌─────────────────────────────────────────┐
│  Claude Code: Edit GDScript files       │
└────────────┬────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────┐
│  Run: ./validate.sh                     │
│  (godot --headless ...)                 │
└────────────┬────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────┐
│  Godot: Validates all .gd files         │
│  Writes: diagnostics/warnings.json      │
└────────────┬────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────┐
│  Claude Code: Read warnings.json        │
└────────────┬────────────────────────────┘
             │
             ▼
        Issues found?
             │
      ┌──────┴──────┐
      │ Yes         │ No
      ▼             ▼
┌──────────┐   ┌─────────┐
│ Fix      │   │ Done ✅ │
│ Issues   │   └─────────┘
└─────┬────┘
      │
      └────── Loop back to validate.sh
```

**Zero human intervention required!**

---

## Quick Reference

### **Essential Commands**

```bash
# Validate all scripts
./validate.sh

# Check results
cat diagnostics/warnings.json | jq '.metadata.total_issues'

# Get Claude hints
cat diagnostics/warnings.json | jq '.claude_hints'

# Validate in CI
./validate.sh && echo "PASS" || echo "FAIL"
```

### **Essential Files**

```
your-project/
├── validate_gdscript.gd    ← Validation script
├── validate.sh             ← Shell wrapper
└── diagnostics/
    ├── warnings.json       ← Claude reads this
    ├── .last_updated       ← Timestamp (not needed for on-demand)
    └── .scan_metadata.json ← Performance info
```

### **Exit Codes**

- `0` = Validation passed (no errors)
- `1` = Validation failed (errors found)

Use exit codes to control Claude Code's workflow!

---

## Next Steps

1. ✅ Create `validate_gdscript.gd` in your project
2. ✅ Create `validate.sh` wrapper
3. ✅ Test: `./validate.sh`
4. ✅ Integrate with Claude Code workflow
5. 🎉 Enjoy zero-intervention GDScript fixing!
