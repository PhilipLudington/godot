#!/bin/bash
# validate.sh - Convenient wrapper for Godot GDScript validation
# Usage: ./validate.sh [project_path]
# Exit codes: 0 = success, 1 = errors found

set -e

# Configuration
GODOT_BIN="${GODOT_BIN:-../bin/godot.macos.editor.arm64}"
PROJECT_DIR="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
SCRIPT_PATH="$(cd "$(dirname "$0")" && pwd)/validate_gdscript.gd"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Running Godot GDScript Validation"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Godot: $GODOT_BIN"
echo "Project: $PROJECT_DIR"
echo "Script: $SCRIPT_PATH"
echo ""

# Check if Godot binary exists
if [ ! -f "$GODOT_BIN" ]; then
    echo -e "${RED}ERROR: Godot binary not found at: $GODOT_BIN${NC}"
    echo "Set GODOT_BIN environment variable or build Godot first"
    exit 1
fi

# Check if project directory exists
if [ ! -d "$PROJECT_DIR" ]; then
    echo -e "${RED}ERROR: Project directory not found: $PROJECT_DIR${NC}"
    exit 1
fi

# Check if validation script exists
if [ ! -f "$SCRIPT_PATH" ]; then
    echo -e "${RED}ERROR: Validation script not found: $SCRIPT_PATH${NC}"
    exit 1
fi

# Run validation
"$GODOT_BIN" --headless --path "$PROJECT_DIR" --script "$SCRIPT_PATH" --quit

EXIT_CODE=$?

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if [ $EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✅ Validation successful - Ready to proceed${NC}"
else
    echo -e "${RED}❌ Validation failed - Check diagnostics/warnings.json${NC}"
    echo ""
    echo "Claude Code should:"
    echo "  1. Read diagnostics/warnings.json"
    echo "  2. Analyze claude_hints for fixing strategy"
    echo "  3. Fix the issues"
    echo "  4. Run ./validate.sh again"
fi

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

exit $EXIT_CODE
