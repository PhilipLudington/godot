#!/bin/bash
# verify_diagnostics.sh - Check if diagnostics module is active

GODOT_BIN="${1:-../bin/godot.macos.editor.arm64}"

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Godot Diagnostics Module Verification"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Binary: $GODOT_BIN"
echo ""

# Check if binary exists
if [ ! -f "$GODOT_BIN" ]; then
    echo "❌ FAILED: Binary not found at $GODOT_BIN"
    exit 1
fi

# Check binary date
echo "📅 Build date:"
ls -lh "$GODOT_BIN" | awk '{print "   " $6, $7, $8}'
echo ""

# Create test script
cat > /tmp/test_diagnostics.gd << 'EOF'
extends SceneTree

func _init():
    var wc = Engine.get_singleton("WarningCapture")
    if wc:
        print("✅ WarningCapture singleton found")
        var auto_enabled = ProjectSettings.get_setting("diagnostics/auto_capture_enabled", null)
        if auto_enabled != null:
            print("✅ Diagnostics settings registered")
            print("✅ Module is ACTIVE and WORKING")
            quit(0)
        else:
            print("❌ Settings not found")
            quit(1)
    else:
        print("❌ WarningCapture singleton NOT FOUND")
        print("❌ Module is NOT active")
        quit(1)
EOF

# Run test
echo "🔍 Testing module..."
OUTPUT=$("$GODOT_BIN" --headless --path /tmp --script /tmp/test_diagnostics.gd --quit 2>&1)
EXIT_CODE=$?

# Extract key messages
echo "$OUTPUT" | grep -E "(WarningCapture singleton|Diagnostics settings|Module is)" | while read line; do
    echo "   $line"
done

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if [ $EXIT_CODE -eq 0 ]; then
    echo "✅ VERIFICATION PASSED"
    echo ""
    echo "The diagnostics module is active and ready to use!"
    echo "You can now run validation on any project."
else
    echo "❌ VERIFICATION FAILED"
    echo ""
    echo "The module is not working. You may need to rebuild:"
    echo "  cd /Users/mrphil/Fun/godot-fork"
    echo "  scons platform=macos arch=arm64 vulkan=no -j8"
fi

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

exit $EXIT_CODE
