# Claude Code Setup Guide for Godot Diagnostics

**Goal:** Enable Claude Code to automatically detect and fix GDScript warnings/errors with zero manual intervention.

---

## Quick Start (5 Minutes)

### 1. Build Godot with Diagnostics Module

```bash
cd /Users/mrphil/Fun/godot-fork
scons platform=macos arch=arm64 vulkan=no -j8
```

**Result:** `bin/godot.macos.editor.arm64` with diagnostics module enabled

### 2. Open Your Game Project

```bash
# Launch the custom Godot build
./bin/godot.macos.editor.arm64

# Open your project (e.g., Stellar Throne)
# File → Open Project → Select your project folder
```

### 3. Verify Auto-Capture is Running

Check the Godot console output on startup:

```
==========================================
WARNING CAPTURE MODULE: Initialized
Auto-capture: ENABLED
Access from console: WarningCapture.capture_debugger_warnings_now()
Warnings will be written to diagnostics/warnings.json
Debugger errors will be written to diagnostics/debugger.json
==========================================
WarningCaptureEditor: Setting up auto-capture event listeners...
WarningCaptureEditor: Connected to filesystem_changed signal
WarningCaptureEditor: Auto-capture enabled - will scan on file saves
```

### 4. Test the System

1. **Edit a GDScript file** - Add a deliberate warning:
   ```gdscript
   # test_script.gd
   extends Node

   func _ready():
       var unused_variable = 10  # This will trigger UNUSED_VARIABLE warning
       print("Hello")
   ```

2. **Save the file** (Cmd+S)

3. **Check console output:**
   ```
   WarningCaptureEditor: Filesystem changed, auto-capturing diagnostics...
   WarningCaptureEditor: Scanning 45 GDScript files (timeout: 500ms per file)...
   WarningCaptureEditor: Scan complete - 45 files in 1234ms
   WarningCaptureEditor: Found 1 issues (0 slow files)
   WarningCaptureEditor: Auto-capture completed in 1250ms
   ```

4. **Verify output files exist:**
   ```bash
   ls -la <your-project-path>/diagnostics/
   # Should show:
   # .last_updated
   # .scan_metadata.json
   # warnings.json
   # debugger.json
   ```

---

## Claude Code Integration

### Method 1: File Watching (Recommended)

Create a simple file watcher that monitors `.last_updated`:

**Option A: Using Node.js (if you have Claude Code with Node)**

```javascript
// watch-godot-diagnostics.js
const fs = require('fs');
const path = require('path');

const PROJECT_PATH = '/path/to/your/godot/project';
const DIAGNOSTICS_DIR = path.join(PROJECT_PATH, 'diagnostics');
const TIMESTAMP_FILE = path.join(DIAGNOSTICS_DIR, '.last_updated');

console.log('Watching Godot diagnostics:', TIMESTAMP_FILE);

let lastTimestamp = 0;

// Poll every 2 seconds
setInterval(() => {
  if (!fs.existsSync(TIMESTAMP_FILE)) return;

  const data = JSON.parse(fs.readFileSync(TIMESTAMP_FILE, 'utf8'));

  if (data.timestamp_ms > lastTimestamp) {
    lastTimestamp = data.timestamp_ms;
    console.log('\n🔔 Diagnostics updated at:', data.datetime);

    // Read and display warnings
    const warnings = JSON.parse(fs.readFileSync(
      path.join(DIAGNOSTICS_DIR, 'warnings.json'), 'utf8'
    ));

    console.log(`Found ${warnings.metadata.total_issues} issues`);
    console.log('Claude hints:', warnings.claude_hints);

    // TODO: Send to Claude Code for analysis
    // analyzeDiagnostics(warnings);
  }
}, 2000);
```

**Option B: Using Python**

```python
#!/usr/bin/env python3
# watch_godot_diagnostics.py
import json
import time
from pathlib import Path

PROJECT_PATH = Path('/path/to/your/godot/project')
DIAGNOSTICS_DIR = PROJECT_PATH / 'diagnostics'
TIMESTAMP_FILE = DIAGNOSTICS_DIR / '.last_updated'

print(f'Watching Godot diagnostics: {TIMESTAMP_FILE}')

last_timestamp = 0

while True:
    if TIMESTAMP_FILE.exists():
        with open(TIMESTAMP_FILE) as f:
            data = json.load(f)

        if data['timestamp_ms'] > last_timestamp:
            last_timestamp = data['timestamp_ms']
            print(f"\n🔔 Diagnostics updated at: {data['datetime']}")

            # Read warnings
            with open(DIAGNOSTICS_DIR / 'warnings.json') as f:
                warnings = json.load(f)

            print(f"Found {warnings['metadata']['total_issues']} issues")
            print(f"Claude hints: {warnings['claude_hints']}")

            # TODO: Send to Claude Code for analysis
            # analyze_diagnostics(warnings)

    time.sleep(2)
```

### Method 2: Manual Trigger

If you prefer to manually trigger Claude Code analysis:

1. **Save your GDScript files** in Godot
2. **Wait 2 seconds** (debounce period)
3. **In Claude Code chat**, paste:
   ```
   Please read and analyze:
   - <project>/diagnostics/warnings.json
   - <project>/diagnostics/.scan_metadata.json

   Focus on the claude_hints section for optimal fixing strategy.
   ```

### Method 3: Claude Code Auto-Detection

Add this to your project's `.clauderc` or MCP configuration:

```json
{
  "watch_files": [
    "diagnostics/.last_updated"
  ],
  "on_file_change": {
    "diagnostics/.last_updated": {
      "action": "analyze_diagnostics",
      "read_files": [
        "diagnostics/warnings.json",
        "diagnostics/.scan_metadata.json"
      ],
      "prompt": "New GDScript diagnostics available. Use claude_hints to prioritize fixes."
    }
  }
}
```

---

## Understanding the Output Files

### 1. `.last_updated` (Timestamp File)

**Purpose:** Claude Code watches this file to detect when diagnostics are updated.

**Format:**
```json
{
  "timestamp_ms": 1731673845123,
  "datetime": "2025-11-15T10:30:45",
  "warnings_file": "diagnostics/warnings.json",
  "debugger_file": "diagnostics/debugger.json",
  "metadata_file": "diagnostics/.scan_metadata.json"
}
```

**Use:** When `timestamp_ms` changes, read the referenced files.

### 2. `warnings.json` (Main Diagnostics)

**Purpose:** Contains all compile-time warnings and parse errors.

**Key Sections:**

```json
{
  "metadata": {
    "total_issues": 23,
    "by_severity": {"error": 2, "warning": 21},
    "by_category": {"unused_variable": 15, "shadowed_variable": 6},
    "by_file": {"res://scripts/player.gd": 5},
    "schema_version": "1.0"
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
      "category": "unused_variable",
      "priority": 1
    }
  ],
  "fix_sequence": [
    "Fix 2 parse/syntax error(s) - blocks compilation",
    "Fix 21 compiler warning(s) - improves code quality"
  ]
}
```

**Claude Code Strategy:**

1. Read `claude_hints.priority_order` → "errors_first"
2. Filter `issues` by `severity: "error"` → fix these first
3. Use `claude_hints.start_with_file` → begin with file containing most issues
4. Check `claude_hints.batch_fixes_available` → fix categories in bulk

**Example Claude Code Workflow:**

```
Step 1: Fix errors (blocks compilation)
  - res://scripts/enemy.gd:12 - Parse error

Step 2: Batch fix unused_variable warnings (15 total)
  - Remove all unused variable declarations

Step 3: Fix remaining warnings individually
  - Rename shadowed variables
```

### 3. `.scan_metadata.json` (Performance Info)

**Purpose:** Track scan performance and identify slow files.

**Format:**
```json
{
  "scan_duration_ms": 12345,
  "total_files_scanned": 450,
  "files_skipped_timeout": 2,
  "scan_timestamp": "2025-11-15T10:30:45",
  "within_budget": true,
  "performance_rating": "good"
}
```

**Use:**
- If `within_budget: false` → increase timeout in Project Settings
- If `files_skipped_timeout > 0` → some files took too long (results may be incomplete)

### 4. `debugger.json` (Runtime Errors)

**Purpose:** Contains errors from game execution (not compile-time).

**Format:** Same as `warnings.json`

**When to use:** After running the game, check for runtime errors.

---

## Configuration Options

All settings are in **Project → Project Settings → Diagnostics**:

### `diagnostics/auto_capture_enabled` (bool)
- **Default:** `true`
- **Description:** Enable/disable automatic capture on file saves
- **Use case:** Disable for very large projects (>1000 files) where scanning is slow

### `diagnostics/debounce_interval_ms` (int)
- **Default:** `2000` (2 seconds)
- **Range:** 500 - 10000 ms
- **Description:** Minimum time between scans (prevents excessive re-scanning)
- **Use case:** Increase to 5000ms if you save files rapidly

### `diagnostics/per_file_timeout_ms` (int)
- **Default:** `500` (0.5 seconds)
- **Range:** 100 - 5000 ms
- **Description:** Maximum time to validate a single file
- **Use case:** Increase to 1000ms for projects with very complex files

### `diagnostics/total_scan_timeout_ms` (int)
- **Default:** `90000` (90 seconds)
- **Range:** 10000 - 300000 ms
- **Description:** Maximum total time for a full project scan
- **Use case:** Adjust based on project size

**Accessing from GDScript:**

```gdscript
# Get current settings
var auto_enabled = ProjectSettings.get_setting("diagnostics/auto_capture_enabled")

# Change at runtime
var wc = WarningCapture
wc.set_auto_capture_enabled(false)  # Temporarily disable
wc.set_debounce_interval(5000)      # 5 second cooldown
```

---

## Troubleshooting

### Problem: Empty JSON files

**Cause:** Auto-capture may be disabled or no files have been saved yet.

**Solution:**
1. Check console for initialization message
2. Verify `diagnostics/auto_capture_enabled = true` in Project Settings
3. Save a `.gd` file to trigger a scan
4. Manually trigger: `WarningCapture.capture_debugger_warnings_now()` in console

### Problem: Files not updating after save

**Cause:** Debounce interval hasn't elapsed.

**Solution:**
1. Wait 2 seconds after save
2. Check console for "Skipping scan (debounce: ...)" message
3. Reduce `diagnostics/debounce_interval_ms` if needed

### Problem: Scan takes too long

**Cause:** Large project or complex files.

**Solution:**
1. Check `.scan_metadata.json` for `scan_duration_ms`
2. Increase `diagnostics/total_scan_timeout_ms`
3. Check `files_skipped_timeout` for slow files
4. Increase `diagnostics/per_file_timeout_ms` if specific files are slow

### Problem: Missing warnings/errors

**Cause:** Timeout exceeded before scanning all files.

**Solution:**
1. Check `.scan_metadata.json`:
   - `within_budget: false` → timeout exceeded
   - `files_skipped_timeout > 0` → some files too slow
2. Increase timeout settings
3. Files are scanned by priority (Core → Scripts → Tools → Tests)
   - Most important files are scanned first

### Problem: Integration tests causing hangs

**Cause:** By design - integration tests often have complex dependency chains.

**Solution:**
- Files in `test/integration/` are automatically skipped
- Check console for "Skipped validation (integration test...)" messages
- This is intentional to prevent validation hangs

---

## Claude Code Workflow Examples

### Example 1: Automatic Fix on File Save

**Setup:** File watcher running

**Workflow:**
1. You edit `player.gd` in Godot
2. Save (Cmd+S)
3. Godot scans all files (2 seconds later)
4. `.last_updated` timestamp changes
5. File watcher detects change
6. Claude Code reads `warnings.json`
7. Claude analyzes issues using `claude_hints`
8. Claude suggests or auto-applies fixes
9. You review and commit

### Example 2: Batch Fix Unused Variables

**Prompt to Claude Code:**

```
Read diagnostics/warnings.json and fix all issues in the
"unused_variable" category. The claude_hints suggest this
category is batch-fixable. Remove all unused variable
declarations across the project.
```

**Claude Response:**
```
I found 15 unused variables across 8 files. I'll remove them:

Files to modify:
- res://scripts/player.gd (3 unused)
- res://scripts/enemy.gd (5 unused)
- res://scripts/ui/menu.gd (7 unused)

Removing all unused variable declarations...
```

### Example 3: Priority-Based Fixing

**Prompt to Claude Code:**

```
Read diagnostics/warnings.json and follow the recommended
fix sequence. Start with errors, then batch-fix warnings,
focusing on the file with the most issues first.
```

**Claude Response:**
```
Following claude_hints recommendations:

Priority 1: Fix 2 parse errors (blocks compilation)
  ✓ res://scripts/enemy.gd:12 - Fixed missing colon

Priority 2: Start with res://scripts/player.gd (5 issues)
  ✓ Removed 3 unused variables
  ✓ Renamed 2 shadowed variables

Priority 3: Batch fix remaining unused_variable warnings
  ✓ Processed 12 more files

All issues resolved. Estimated time: 5-15 minutes (actual: 8 min)
```

---

## Advanced: MCP Server Integration

If you're using Claude Code with MCP (Model Context Protocol):

### Create a Godot Diagnostics MCP Server

```typescript
// godot-diagnostics-mcp/index.ts
import { Server } from '@modelcontextprotocol/sdk/server';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio';
import fs from 'fs/promises';
import path from 'path';

const PROJECT_PATH = process.env.GODOT_PROJECT_PATH || process.cwd();
const DIAGNOSTICS_DIR = path.join(PROJECT_PATH, 'diagnostics');

const server = new Server({
  name: 'godot-diagnostics',
  version: '1.0.0'
});

// Tool: Get latest diagnostics
server.tool('get_diagnostics', async () => {
  const warnings = JSON.parse(
    await fs.readFile(path.join(DIAGNOSTICS_DIR, 'warnings.json'), 'utf8')
  );
  return {
    content: [{
      type: 'text',
      text: JSON.stringify(warnings, null, 2)
    }]
  };
});

// Tool: Watch for updates
server.tool('watch_diagnostics', async () => {
  const timestamp = JSON.parse(
    await fs.readFile(path.join(DIAGNOSTICS_DIR, '.last_updated'), 'utf8')
  );
  return {
    content: [{
      type: 'text',
      text: `Last updated: ${timestamp.datetime}`
    }]
  };
});

// Start server
const transport = new StdioServerTransport();
server.connect(transport);
```

**Register in Claude Code config:**

```json
{
  "mcpServers": {
    "godot-diagnostics": {
      "command": "node",
      "args": ["godot-diagnostics-mcp/index.ts"],
      "env": {
        "GODOT_PROJECT_PATH": "/path/to/your/project"
      }
    }
  }
}
```

---

## Performance Tips

### For Small Projects (<100 files)
- Default settings work great
- Full scans complete in <5 seconds
- No optimization needed

### For Medium Projects (100-500 files)
- Consider increasing `debounce_interval_ms` to 3000-5000ms
- Watch `.scan_metadata.json` for performance
- Most scans should stay under 30 seconds

### For Large Projects (>500 files)
- Increase `total_scan_timeout_ms` to 120000-180000ms (2-3 minutes)
- Increase `debounce_interval_ms` to 5000-10000ms
- Consider disabling auto-capture and running manually:
  ```gdscript
  # Disable auto-capture
  WarningCapture.set_auto_capture_enabled(false)

  # Run manually when needed
  WarningCapture.capture_debugger_warnings_now()
  ```

---

## Summary Checklist

**Initial Setup:**
- [ ] Build custom Godot with diagnostics module
- [ ] Launch editor and verify module initialization
- [ ] Check Project Settings → Diagnostics (all settings present)
- [ ] Test by saving a file with a warning
- [ ] Verify `diagnostics/` folder created with 4 files

**Claude Code Integration:**
- [ ] Choose integration method (file watcher recommended)
- [ ] Set up file watcher or MCP server
- [ ] Test detection of `.last_updated` changes
- [ ] Verify Claude can read and parse `warnings.json`
- [ ] Test Claude's ability to fix issues using `claude_hints`

**Optimization:**
- [ ] Monitor `.scan_metadata.json` for performance
- [ ] Adjust timeout settings if needed
- [ ] Configure debounce interval based on workflow

**Ready!** Claude Code can now automatically detect and fix GDScript issues! 🎉

---

## Support

**Console Commands:**
```gdscript
# Manual trigger
WarningCapture.capture_debugger_warnings_now()

# Check all issues
WarningCapture.get_all_warnings_and_errors()

# Toggle auto-capture
WarningCapture.set_auto_capture_enabled(false)
WarningCapture.set_auto_capture_enabled(true)
```

**Log Monitoring:**
- Check Godot console for "WarningCaptureEditor:" messages
- Look for scan duration and file counts
- Watch for timeout warnings

**Files to Monitor:**
- `diagnostics/.last_updated` - Timestamp changes
- `diagnostics/warnings.json` - Main diagnostic output
- `diagnostics/.scan_metadata.json` - Performance metrics
