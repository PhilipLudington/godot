@tool
extends EditorPlugin

# This version of the plugin uses the C++ WarningCaptureEditor module
# To use this, you need to build Godot with the warning_capture module

var timer: Timer
var error_log_file: String = "res://godot_errors.json"
var warning_capture: RefCounted  # Will hold WarningCaptureEditor instance

func _enter_tree() -> void:
	print("[WarningCapture] Plugin with C++ module starting...")

	# Try to create the C++ module instance
	if ClassDB.class_exists("WarningCaptureEditor"):
		warning_capture = ClassDB.instantiate("WarningCaptureEditor")
		print("[WarningCapture] C++ module loaded successfully!")

		# Set up periodic checking
		timer = Timer.new()
		timer.wait_time = 2.0
		timer.timeout.connect(_capture_errors_with_module)
		timer.autostart = true
		add_child(timer)

		# Initial capture
		_capture_errors_with_module()
	else:
		print("[WarningCapture] ERROR: WarningCaptureEditor class not found!")
		print("[WarningCapture] Make sure Godot was built with the warning_capture module")
		print("[WarningCapture] Build with: scons platform=macos arch=arm64 vulkan=no -j8")

func _exit_tree() -> void:
	if timer:
		timer.queue_free()
		timer = null
	warning_capture = null
	print("[WarningCapture] Plugin stopped")

func _capture_errors_with_module() -> void:
	if not warning_capture:
		return

	# Get all warnings and errors from the debugger
	var errors = warning_capture.get_all_warnings_and_errors()
	print("[WarningCapture] Found %d errors/warnings" % errors.size())

	if errors.size() > 0:
		var output = []
		for error in errors:
			var error_dict = {
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

			# Print details for important warnings
			if error_dict["source_file"] != "":
				print("[WarningCapture] %s at %s:%d" % [
					error_dict["message"],
					error_dict["source_file"],
					error_dict["source_line"]
				])

		# Save to file
		warning_capture.dump_errors_to_file(error_log_file)
		print("[WarningCapture] Saved errors to %s" % error_log_file)

		# Also save in our custom format
		var file = FileAccess.open(error_log_file.replace(".json", "_detailed.json"), FileAccess.WRITE)
		if file:
			var json_string = JSON.stringify(output, "\t")
			file.store_string(json_string)
			file.close()