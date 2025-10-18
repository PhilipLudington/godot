@tool
extends EditorPlugin

var timer: Timer
var error_log_file: String = "res://godot_errors.json"
var last_error_count: int = 0

func _enter_tree() -> void:
	print("[WarningCapture] Plugin started")

	# Set up a timer to periodically check for errors
	timer = Timer.new()
	timer.wait_time = 2.0
	timer.timeout.connect(_capture_errors)
	timer.autostart = true
	add_child(timer)

	# Capture errors immediately on start
	_capture_errors()

func _exit_tree() -> void:
	if timer:
		timer.queue_free()
		timer = null

func _capture_errors() -> void:
	var editor_interface = EditorInterface
	if not editor_interface:
		print("[WarningCapture] EditorInterface not available")
		return

	var debugger_node = editor_interface.get_debugger_node()
	if not debugger_node:
		print("[WarningCapture] DebuggerNode not available")
		return

	var default_debugger = debugger_node.get_default_debugger()
	if not default_debugger:
		print("[WarningCapture] Default debugger not available")
		return

	var errors = default_debugger.get_all_errors()
	if errors.size() != last_error_count:
		print("[WarningCapture] Found %d errors/warnings (changed from %d)" % [errors.size(), last_error_count])
		last_error_count = errors.size()

		if errors.size() > 0:
			var output = []
			var idx = 0
			for error in errors:
				idx += 1
				var error_dict = {
					"index": idx,
					"time": error.get("time", ""),
					"message": error.get("message", ""),
					"is_warning": error.get("is_warning", false),
					"is_error": error.get("is_error", false),
					"source_file": error.get("source_file", ""),
					"source_line": error.get("source_line", 0),
					"source_info": error.get("source_info", ""),
					"error_condition": error.get("error_condition", "")
				}
				output.append(error_dict)

				# Print details for debugging
				print("[WarningCapture] Error #%d:" % idx)
				print("  Message: %s" % error_dict["message"])
				print("  File: %s" % error_dict["source_file"])
				print("  Line: %d" % error_dict["source_line"])
				print("  Is Warning: %s" % error_dict["is_warning"])
				print("  Is Error: %s" % error_dict["is_error"])
				if error_dict["error_condition"]:
					print("  Condition: %s" % error_dict["error_condition"])
				print("---")

			# Save to file
			var file = FileAccess.open(error_log_file, FileAccess.WRITE)
			if file:
				var json_string = JSON.stringify(output, "\t")
				file.store_string(json_string)
				file.close()
				print("[WarningCapture] Saved %d errors to %s" % [output.size(), error_log_file])
			else:
				print("[WarningCapture] Failed to open file: %s" % error_log_file)