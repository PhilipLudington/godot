# Headless Mode Support for Diagnostics Module

## Current Limitations

The diagnostics module **partially works** in headless mode:

| Feature | GUI Mode | Headless Mode |
|---------|----------|---------------|
| Module initialization | ✅ Yes | ✅ Yes |
| Manual `capture_debugger_warnings_now()` | ✅ Yes | ✅ Yes |
| GDScript validation | ✅ Yes | ✅ Yes |
| Auto-capture on file save | ✅ Yes | ❌ No |
| Filesystem watching | ✅ Yes | ❌ No |

**Reason:** Auto-capture depends on `EditorNode` and `EditorFileSystem` which don't exist in headless mode.

## Workarounds

### Option 1: Manual Validation Script (Recommended)

**Use case:** CI/CD pipelines, pre-commit hooks

```bash
# Run validation
./godot --headless --path /path/to/project --script validate.gd --quit

# Check exit code
if [ $? -eq 0 ]; then
    echo "✅ Validation passed"
else
    echo "❌ Validation failed"
fi
```

**validate.gd:**
```gdscript
extends SceneTree

func _init():
    var wc = WarningCapture
    wc.capture_debugger_warnings_now()

    # Read results
    var file = FileAccess.open("res://diagnostics/warnings.json", FileAccess::READ)
    var json = JSON.parse_string(file.get_as_text())

    var total = json["metadata"]["total_issues"]
    var errors = json["metadata"]["by_severity"].get("error", 0)

    print("Found %d issues (%d errors)" % [total, errors])

    # Exit with error code if issues found
    if errors > 0:
        quit(1)  # Non-zero exit code
    else:
        quit(0)
```

### Option 2: External File Watcher + Manual Trigger

**Use case:** Development server watching for changes

```bash
# Terminal 1: Run Godot in server mode
./godot --headless --path /path/to/project

# Terminal 2: Watch for file changes
fswatch -0 /path/to/project/**/*.gd | xargs -0 -n1 ./trigger_validation.sh
```

**trigger_validation.sh:**
```bash
#!/bin/bash
# Send signal to Godot to re-validate
# (Would need custom signal handling)
```

### Option 3: Periodic Validation

**Use case:** Background validation service

```gdscript
# server_validator.gd
extends SceneTree

var timer_interval = 10.0  # Re-validate every 10 seconds

func _init():
    var timer = Timer.new()
    timer.wait_time = timer_interval
    timer.timeout.connect(_on_timeout)
    root.add_child(timer)
    timer.start()

func _on_timeout():
    print("Re-validating GDScript files...")
    WarningCapture.capture_debugger_warnings_now()
```

```bash
# Run as long-running service
./godot --headless --path /path/to/project --script server_validator.gd
```

## Future Enhancement: Full Headless Support

To make auto-capture work in headless mode, modify the code:

### Proposed Changes

**File:** `modules/warning_capture/editor/warning_capture_editor.cpp`

```cpp
void WarningCaptureEditor::_setup_event_listeners() {
    // Check if we're in headless mode
    bool is_headless = DisplayServer::get_singleton()->window_get_mode() == DisplayServer::WINDOW_MODE_HEADLESS;

    if (is_headless) {
        print_line("WarningCaptureEditor: Headless mode - using filesystem polling");
        _setup_headless_file_watcher();
        return;
    }

    // Normal GUI mode setup
    if (!EditorNode::get_singleton()) {
        print_line("WarningCaptureEditor: EditorNode not available");
        return;
    }

    // ... existing code
}

void WarningCaptureEditor::_setup_headless_file_watcher() {
    // Use DirAccess to watch for file changes
    // Poll every 2 seconds for .gd file modifications

    Timer *poll_timer = memnew(Timer);
    poll_timer->set_wait_time(2.0);
    poll_timer->set_autostart(true);
    poll_timer->timeout.connect(callable_mp(this, &WarningCaptureEditor::_check_for_file_changes));
    add_child(poll_timer);
}

void WarningCaptureEditor::_check_for_file_changes() {
    // Scan for modified .gd files
    // If found, trigger capture_debugger_warnings_now()
}
```

**Complexity:** Medium
**Benefit:** Full auto-capture in headless CI/CD environments
**Tradeoff:** Polling is less efficient than event-driven

## Recommended Approach

**For CI/CD:** Use **Option 1** (Manual validation script)
- Simple, reliable, efficient
- No code changes needed
- Easy to integrate with CI systems

**For development server:** Use **Option 3** (Periodic validation)
- Works with current code
- Good enough for most use cases
- Can adjust polling interval

**For production headless:** Consider implementing full headless support if there's demand.

## CI/CD Integration Examples

### GitHub Actions

```yaml
name: Validate GDScript
on: [push, pull_request]

jobs:
  validate:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Download Godot
        run: |
          wget https://your-godot-build.tar.gz
          tar xzf godot.tar.gz

      - name: Validate Scripts
        run: |
          ./godot --headless --path . --script .github/validate.gd --quit

      - name: Check Results
        run: |
          ISSUES=$(jq '.metadata.total_issues' diagnostics/warnings.json)
          if [ "$ISSUES" -gt 0 ]; then
            echo "::error::Found $ISSUES GDScript issues"
            exit 1
          fi
```

### GitLab CI

```yaml
validate_gdscript:
  stage: test
  script:
    - ./godot --headless --path . --script ci_validate.gd --quit
    - |
      ISSUES=$(jq '.metadata.total_issues' diagnostics/warnings.json)
      if [ "$ISSUES" -gt 0 ]; then
        echo "Found $ISSUES issues"
        exit 1
      fi
  artifacts:
    when: on_failure
    paths:
      - diagnostics/
```

### Pre-commit Hook

```bash
#!/bin/bash
# .git/hooks/pre-commit

# Only validate staged .gd files
STAGED=$(git diff --cached --name-only --diff-filter=ACM | grep '\.gd$')

if [ -n "$STAGED" ]; then
    echo "Validating GDScript files..."
    ./godot --headless --path . --script .git/hooks/validate.gd --quit

    if [ $? -ne 0 ]; then
        echo "❌ GDScript validation failed. Fix issues before committing."
        exit 1
    fi
fi
```

## Summary

| Mode | Auto-Capture | Manual Trigger | Best For |
|------|--------------|----------------|----------|
| **GUI** | ✅ Yes | ✅ Yes | Interactive development |
| **Headless (current)** | ❌ No | ✅ Yes | CI/CD, batch validation |
| **Headless (enhanced)** | ⚠️ Polling | ✅ Yes | Development servers |

**Recommendation:** For now, use manual triggering in headless mode. It's simple, reliable, and covers most CI/CD use cases.
