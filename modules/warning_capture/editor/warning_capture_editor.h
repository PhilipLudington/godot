#ifndef WARNING_CAPTURE_EDITOR_H
#define WARNING_CAPTURE_EDITOR_H

#include "core/object/ref_counted.h"
#include "core/variant/typed_array.h"

class Script;

class WarningCaptureEditor : public RefCounted {
	GDCLASS(WarningCaptureEditor, RefCounted);

protected:
	static void _bind_methods();

private:
	// File path for warnings output
	String warnings_file_path;
	String debugger_file_path;
	String timestamp_file_path;
	String metadata_file_path;

	// In-memory storage for current session's warnings
	TypedArray<Dictionary> accumulated_warnings;
	TypedArray<Dictionary> accumulated_debugger_errors;

	// Auto-capture state
	bool auto_capture_enabled = true;
	uint64_t last_scan_timestamp = 0;
	uint64_t debounce_interval_ms = 2000; // 2 second cooldown
	uint64_t per_file_timeout_ms = 500; // 500ms max per file
	uint64_t total_scan_timeout_ms = 90000; // 90 second total budget

	// Helper methods
	void _capture_debugger_warnings();
	void _capture_startup_errors();
	TypedArray<Dictionary> _get_gdscript_parse_errors() const;
	TypedArray<Dictionary> _get_editor_log_messages() const;
	void _write_warnings_file();
	void _write_debugger_file();
	void _update_warning_metadata();
	Dictionary _categorize_error(const Dictionary &p_error) const;
	TypedArray<Dictionary> _get_debugger_errors() const;

	// GDScript validation methods
	TypedArray<Dictionary> _scan_all_gdscripts();
	void _find_all_gdscript_files(const String &p_dir, Vector<String> &r_files);

	// Event handlers (NEW)
	void _on_filesystem_changed();
	void _on_script_saved(Ref<Script> p_script);
	void _setup_event_listeners();

	// Helper methods (NEW)
	void _write_timestamp_file();
	void _write_metadata_file(uint64_t p_duration_ms, int p_files_scanned, int p_files_skipped);
	bool _should_skip_scan();
	String _estimate_fix_time(int p_issue_count) const;

public:
	TypedArray<Dictionary> get_all_warnings_and_errors() const;
	void dump_errors_to_file(const String &p_file_path) const;
	void clear_errors();
	void add_warning(const Dictionary &p_warning);
	void capture_debugger_warnings_now();  // Call this after scripts are loaded
	void capture_debugger_errors_now();    // Capture debugger errors to debugger.json

	// Configuration methods (NEW)
	void set_auto_capture_enabled(bool p_enabled);
	void set_debounce_interval(uint64_t p_ms);
	void set_per_file_timeout(uint64_t p_ms);

	WarningCaptureEditor();
	~WarningCaptureEditor();
};

#endif // WARNING_CAPTURE_EDITOR_H