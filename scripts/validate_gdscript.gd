# validate_gdscript.gd
# Run with: godot --headless --path /path/to/project --script validate_gdscript.gd --quit
# Exit codes: 0 = success, 1 = errors found

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

		# Show first few issues
		var issues = data.get("issues", [])
		var show_count = min(5, issues.size())
		if show_count > 0:
			print("\nFirst %d issues:" % show_count)
			for i in range(show_count):
				var issue = issues[i]
				var severity_icon = "⚠️ " if issue.get("severity") == "warning" else "❌"
				print("  %s %s:%d - %s" % [
					severity_icon,
					issue.get("file", "unknown"),
					issue.get("line", 0),
					issue.get("message", "no message")
				])

	print("=" * 60)
	print("Diagnostics written to: diagnostics/warnings.json")
	print("=" * 60)

	# Exit with error code based on results
	if errors > 0:
		print("\n❌ VALIDATION FAILED - Errors found (blocks compilation)")
		quit(1)
	elif warnings > 0:
		print("\n⚠️  VALIDATION PASSED - Warnings found (should be fixed)")
		quit(0)  # Change to quit(1) if you want warnings to fail CI
	else:
		print("\n✅ VALIDATION PASSED - No issues found")
		quit(0)
