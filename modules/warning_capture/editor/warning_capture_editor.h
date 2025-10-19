#ifndef WARNING_CAPTURE_EDITOR_H
#define WARNING_CAPTURE_EDITOR_H

#include "core/object/ref_counted.h"
#include "core/variant/typed_array.h"

class WarningCaptureEditor : public RefCounted {
	GDCLASS(WarningCaptureEditor, RefCounted);

protected:
	static void _bind_methods();

private:
	// File path for warnings output
	String warnings_file_path;

	// In-memory storage for current session's warnings
	TypedArray<Dictionary> accumulated_warnings;

	// Helper methods
	void _capture_debugger_warnings();
	void _capture_startup_errors();
	TypedArray<Dictionary> _get_gdscript_parse_errors() const;
	TypedArray<Dictionary> _get_editor_log_messages() const;
	void _write_warnings_file();
	void _update_warning_metadata();
	Dictionary _categorize_error(const Dictionary &p_error) const;

	// GDScript validation methods (NEW - for capturing ALL warnings)
	TypedArray<Dictionary> _scan_all_gdscripts();
	void _find_all_gdscript_files(const String &p_dir, Vector<String> &r_files);

	// Debugger error capture (separate from warnings)
	String debugger_file_path;
	TypedArray<Dictionary> accumulated_debugger_errors;
	void _write_debugger_file();
	TypedArray<Dictionary> _get_debugger_errors() const;

public:
	TypedArray<Dictionary> get_all_warnings_and_errors() const;
	void dump_errors_to_file(const String &p_file_path) const;
	void clear_errors();
	void add_warning(const Dictionary &p_warning);
	void capture_debugger_warnings_now();  // Call this after scripts are loaded
	void capture_debugger_errors_now();    // NEW: Capture debugger errors to debugger.json

	WarningCaptureEditor();
	~WarningCaptureEditor();
};

#endif // WARNING_CAPTURE_EDITOR_H