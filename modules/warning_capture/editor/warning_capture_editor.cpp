#include "warning_capture_editor.h"

#include "editor/editor_node.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor/editor_log.h"
#include "core/io/file_access.h"
#include "core/io/dir_access.h"
#include "core/io/json.h"
#include "core/os/time.h"
#include "core/config/project_settings.h"
#include "modules/gdscript/gdscript.h"

void WarningCaptureEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_all_warnings_and_errors"), &WarningCaptureEditor::get_all_warnings_and_errors);
	ClassDB::bind_method(D_METHOD("dump_errors_to_file", "file_path"), &WarningCaptureEditor::dump_errors_to_file);
	ClassDB::bind_method(D_METHOD("clear_errors"), &WarningCaptureEditor::clear_errors);
	ClassDB::bind_method(D_METHOD("add_warning", "warning"), &WarningCaptureEditor::add_warning);
	ClassDB::bind_method(D_METHOD("capture_debugger_warnings_now"), &WarningCaptureEditor::capture_debugger_warnings_now);
}

WarningCaptureEditor::WarningCaptureEditor() {
	// Initialize warnings file path - will be set to godot/diagnostics/warnings.json
	// Use ProjectSettings to get the project directory
	String project_dir = ProjectSettings::get_singleton()->globalize_path("res://");
	String diagnostics_dir = project_dir.path_join("diagnostics");
	warnings_file_path = diagnostics_dir.path_join("warnings.json");

	// Ensure diagnostics directory exists
	DirAccess::make_dir_recursive_absolute(diagnostics_dir);

	// Clear the file on startup (fresh session)
	Ref<FileAccess> file = FileAccess::open(warnings_file_path, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string("{}");
		file->close();
	}

	// Note: Can't capture debugger warnings here - they're not populated yet
	// The error handler will capture parse errors, and we can query debugger warnings
	// later via add_warning() calls or on-demand queries
}

void WarningCaptureEditor::capture_debugger_warnings_now() {
	print_line("WarningCaptureEditor: capture_debugger_warnings_now() called");
	accumulated_warnings.clear();

	// 1. Get parse/compile errors from debugger error_tree
	print_line("WarningCaptureEditor: Getting debugger errors...");
	TypedArray<Dictionary> debugger_errors = get_all_warnings_and_errors();
	print_line("WarningCaptureEditor: Found " + itos(debugger_errors.size()) + " debugger errors");
	for (int i = 0; i < debugger_errors.size(); i++) {
		Dictionary categorized = _categorize_error(debugger_errors[i]);
		accumulated_warnings.append(categorized);
	}

	// 2. Get ALL GDScript warnings by validating all .gd files directly
	print_line("WarningCaptureEditor: Starting GDScript validation...");
	TypedArray<Dictionary> gdscript_warnings = _scan_all_gdscripts();
	print_line("WarningCaptureEditor: GDScript validation complete, categorizing...");
	for (int i = 0; i < gdscript_warnings.size(); i++) {
		Dictionary categorized = _categorize_error(gdscript_warnings[i]);
		accumulated_warnings.append(categorized);
	}

	// 3. Write combined file with both error sources
	print_line("WarningCaptureEditor: Writing warnings file...");
	_write_warnings_file();
	print_line("WarningCaptureEditor: COMPLETE! Captured " + itos(accumulated_warnings.size()) + " total warnings");
}

void WarningCaptureEditor::_capture_debugger_warnings() {
	// Try to get ScriptEditorDebugger to capture compiler warnings
	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	if (!debugger_node) {
		print_line("WarningCaptureEditor: EditorDebuggerNode not found");
		return;
	}

	ScriptEditorDebugger *default_debugger = debugger_node->get_default_debugger();
	if (!default_debugger) {
		print_line("WarningCaptureEditor: ScriptEditorDebugger not found");
		return;
	}

	// Get all errors/warnings from debugger (includes GDScript compiler warnings)
	TypedArray<Dictionary> debugger_errors = default_debugger->get_all_errors();
	print_line("WarningCaptureEditor: Debugger has " + itos(debugger_errors.size()) + " errors/warnings");

	for (int i = 0; i < debugger_errors.size(); i++) {
		Dictionary err = debugger_errors[i];
		// Only add if not already in accumulated warnings
		String file = err.get("file", "");
		Variant line_var = err.get("line", 0);
		int line = (int)line_var;
		String message = err.get("message", "");

		bool is_duplicate = false;
		for (int j = 0; j < accumulated_warnings.size(); j++) {
			Dictionary existing = accumulated_warnings[j];
			String ex_file = existing.get("file", "");
			Variant ex_line_var = existing.get("line", 0);
			int ex_line = (int)ex_line_var;
			String ex_message = existing.get("message", "");

			if (ex_file == file && ex_line == line && ex_message == message) {
				is_duplicate = true;
				break;
			}
		}

		if (!is_duplicate) {
			accumulated_warnings.append(err);
		}
	}

	// Write the file with captured warnings
	if (!accumulated_warnings.is_empty()) {
		_write_warnings_file();
	}
}

void WarningCaptureEditor::_capture_startup_errors() {
	// Try to get EditorLog to capture any parse/compile errors from startup
	EditorNode *editor = EditorNode::get_singleton();
	if (!editor) {
		print_line("WarningCaptureEditor: EditorNode not available yet");
		return;
	}

	EditorLog *log = editor->get_log();
	if (!log) {
		print_line("WarningCaptureEditor: EditorLog not available yet");
		return;
	}

	// Get all messages from EditorLog (includes parse errors)
	TypedArray<Dictionary> log_messages = log->get_all_messages();
	print_line("WarningCaptureEditor: EditorLog has " + itos(log_messages.size()) + " messages");

	for (int i = 0; i < log_messages.size(); i++) {
		Dictionary msg = log_messages[i];
		// Filter for error messages containing "Parse Error" or "Compile Error"
		String message = msg.get("message", "");
		if (message.contains("Parse Error") || message.contains("Compile Error")) {
			// Add to accumulated warnings
			accumulated_warnings.append(msg);
			print_line("WarningCaptureEditor: Captured error: " + message);
		}
	}

	// Write the file with any captured startup errors
	if (!accumulated_warnings.is_empty()) {
		print_line("WarningCaptureEditor: Writing " + itos(accumulated_warnings.size()) + " startup errors");
		_write_warnings_file();
	}
}

WarningCaptureEditor::~WarningCaptureEditor() {
	// Write final warnings file when shutting down
	_write_warnings_file();
}

TypedArray<Dictionary> WarningCaptureEditor::get_all_warnings_and_errors() const {
	// DIRECTLY query ScriptEditorDebugger - it has ALL warnings with file/line metadata
	// This is the ONLY reliable source for compiler warnings (SHADOWED_VARIABLE, UNUSED_VARIABLE, etc.)

	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	if (!debugger_node) {
		return TypedArray<Dictionary>();
	}

	ScriptEditorDebugger *default_debugger = debugger_node->get_default_debugger();
	if (!default_debugger) {
		return TypedArray<Dictionary>();
	}

	// Return ALL errors from debugger (includes parse errors, compile errors, and compiler warnings)
	return default_debugger->get_all_errors();
}

Dictionary WarningCaptureEditor::_categorize_error(const Dictionary &p_error) const {
	// Extract error info - handle both sources:
	// 1. Error handler: has "file", "line", "message"
	// 2. ScriptEditorDebugger: has "source_file", "source_line", "message", "is_warning", "is_error"

	String message = p_error.get("message", "");
	String file = p_error.get("file", "");
	int line = p_error.get("line", 0);

	// Fallback to debugger format if needed
	if (file.is_empty()) {
		file = p_error.get("source_file", "");
	}
	if (line == 0) {
		Variant line_var = p_error.get("source_line", 0);
		line = (int)line_var;
	}

	// Determine severity from is_warning/is_error flags or message content
	String severity = "warning";
	Variant is_error_var = p_error.get("is_error", false);
	Variant is_warning_var = p_error.get("is_warning", false);
	bool is_error = (bool)is_error_var;
	bool is_warning = (bool)is_warning_var;

	if (is_error || message.contains("Parse Error") || message.contains("Compile Error")) {
		severity = "error";
	} else if (is_warning) {
		severity = "warning";
	}

	// Categorize by message content
	String category = "general_error";
	if (message.contains("Parse Error")) {
		category = "parse_error";
	} else if (message.contains("Compile Error")) {
		category = "compile_error";
	} else if (message.contains("UNUSED")) {
		category = "unused_variable";
	} else if (message.contains("SHADOWED")) {
		category = "shadowed_variable";
	}

	// Create categorized error entry
	Dictionary categorized;
	categorized["file"] = file;
	categorized["line"] = line;
	categorized["message"] = message;
	categorized["severity"] = severity;
	categorized["category"] = category;
	categorized["source"] = "compiler";
	categorized["suggested_agent"] = "expert-debugger";

	return categorized;
}

void WarningCaptureEditor::add_warning(const Dictionary &p_warning) {
	// Skip duplicates
	String new_file = p_warning.get("file", "");
	Variant new_line_var = p_warning.get("line", 0);
	int new_line = (int)new_line_var;
	String new_message = p_warning.get("message", "");
	if (new_message.is_empty()) {
		new_message = p_warning.get("text", "");
	}

	for (int i = 0; i < accumulated_warnings.size(); i++) {
		Dictionary existing = accumulated_warnings[i];
		String ex_file = existing.get("file", "");
		Variant ex_line_var = existing.get("line", 0);
		int ex_line = (int)ex_line_var;
		String ex_message = existing.get("message", "");

		if (ex_file == new_file && ex_line == new_line && ex_message == new_message) {
			return;  // Duplicate found
		}
	}

	// Categorize and add the warning
	Dictionary categorized = _categorize_error(p_warning);
	accumulated_warnings.append(categorized);

	// Write to file immediately
	_write_warnings_file();
}

void WarningCaptureEditor::_update_warning_metadata() {
	// This method is unused, but kept for compatibility
	// Metadata is generated in _write_warnings_file()
}

void WarningCaptureEditor::_write_warnings_file() {
	if (warnings_file_path.is_empty()) {
		return;
	}

	// Update metadata counts
	_update_warning_metadata();

	// Sort warnings by severity then file/line
	TypedArray<Dictionary> sorted_warnings = accumulated_warnings;

	// Count by category
	Dictionary by_severity;
	Dictionary by_source;
	Dictionary by_category;

	for (int i = 0; i < sorted_warnings.size(); i++) {
		Dictionary warning = sorted_warnings[i];

		String severity = warning.get("severity", "unknown");
		Variant current_sev = by_severity.get(severity, 0);
		int sev_count = (int)current_sev;
		by_severity[severity] = sev_count + 1;

		String source = warning.get("source", "unknown");
		Variant current_src = by_source.get(source, 0);
		int src_count = (int)current_src;
		by_source[source] = src_count + 1;

		String category = warning.get("category", "unknown");
		Variant current_cat = by_category.get(category, 0);
		int cat_count = (int)current_cat;
		by_category[category] = cat_count + 1;
	}

	// Add priorities
	for (int i = 0; i < sorted_warnings.size(); i++) {
		Dictionary warning = sorted_warnings[i];
		warning["priority"] = i + 1;
		sorted_warnings[i] = warning;
	}

	// Generate fix sequence
	Array fix_sequence;
	Variant err_var = by_severity.get("error", 0);
	Variant warn_var = by_severity.get("warning", 0);
	int error_count = (int)err_var;
	int warning_count = (int)warn_var;
	if (error_count > 0) {
		fix_sequence.append(vformat("Fix %d error(s)", error_count));
	}
	if (warning_count > 0) {
		fix_sequence.append(vformat("Fix %d warning(s)", warning_count));
	}

	// Build final report
	Dictionary metadata;
	metadata["generated_at"] = Time::get_singleton()->get_datetime_string_from_system();
	metadata["project"] = "Stellar Throne";
	metadata["total_issues"] = sorted_warnings.size();
	metadata["by_source"] = by_source;
	metadata["by_severity"] = by_severity;
	metadata["by_category"] = by_category;

	Dictionary report;
	report["metadata"] = metadata;
	report["issues"] = sorted_warnings;
	report["fix_sequence"] = fix_sequence;

	// Write to file
	Ref<FileAccess> file = FileAccess::open(warnings_file_path, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(JSON::stringify(report));
		file->close();
	}
}

TypedArray<Dictionary> WarningCaptureEditor::_get_gdscript_parse_errors() const {
	TypedArray<Dictionary> result;
	return _get_editor_log_messages();
}

TypedArray<Dictionary> WarningCaptureEditor::_get_editor_log_messages() const {
	TypedArray<Dictionary> result;

	EditorNode *editor = EditorNode::get_singleton();
	if (!editor) {
		return result;
	}

	EditorLog *log = editor->get_log();
	if (!log) {
		return result;
	}

	result = log->get_all_messages();
	return result;
}

void WarningCaptureEditor::dump_errors_to_file(const String &p_file_path) const {
	TypedArray<Dictionary> errors = get_all_warnings_and_errors();

	if (errors.is_empty()) {
		print_line("No errors/warnings to dump");
		return;
	}

	Ref<FileAccess> file = FileAccess::open(p_file_path, FileAccess::WRITE);
	if (file.is_null()) {
		ERR_PRINT("Failed to open file: " + p_file_path);
		return;
	}

	for (int i = 0; i < errors.size(); i++) {
		Dictionary error = errors[i];
		String json_line = JSON::stringify(error);
		file->store_line(json_line);
	}

	file->close();
	print_line("Dumped " + itos(errors.size()) + " errors/warnings to " + p_file_path);
}

void WarningCaptureEditor::clear_errors() {
	accumulated_warnings.clear();
	_write_warnings_file();
}

// NEW: Scan all GDScript files and collect warnings via direct validation
void WarningCaptureEditor::_find_all_gdscript_files(const String &p_dir, Vector<String> &r_files) {
	Ref<DirAccess> dir = DirAccess::open(p_dir);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	String file_name = dir->get_next();

	while (!file_name.is_empty()) {
		if (dir->current_is_dir()) {
			// Skip hidden directories and addons
			if (!file_name.begins_with(".") && file_name != "addons") {
				String sub_dir = p_dir.path_join(file_name);
				_find_all_gdscript_files(sub_dir, r_files);
			}
		} else {
			// Add .gd files
			if (file_name.ends_with(".gd")) {
				String full_path = p_dir.path_join(file_name);
				r_files.push_back(full_path);
			}
		}
		file_name = dir->get_next();
	}

	dir->list_dir_end();
}

// Comparator for sorting files by validation priority
struct _ValidationPriorityComparator {
	_FORCE_INLINE_ bool operator()(const String &a, const String &b) const {
		// Get priority for each file (lower number = higher priority)
		int priority_a = _get_priority(a);
		int priority_b = _get_priority(b);

		// Sort by priority first, then alphabetically within same priority
		if (priority_a != priority_b) {
			return priority_a < priority_b;
		}
		return a < b;
	}

	static int _get_priority(const String &path) {
		// Priority 0: Core game code (scripts/, scenes/)
		if (path.contains("/scripts/") || path.contains("/scenes/")) {
			// Extra priority for autoloads and core systems
			if (path.contains("/autoload/") || path.contains("/core/")) {
				return 0;
			}
			return 1;
		}

		// Priority 2: Tools and utilities
		if (path.contains("/tools/") || path.contains("/addons/")) {
			return 2;
		}

		// Priority 3: Unit tests
		if (path.contains("/test/unit/")) {
			return 3;
		}

		// Priority 4: Integration tests (slowest, validated last)
		if (path.contains("/test/integration/")) {
			return 4;
		}

		// Priority 5: Everything else
		return 5;
	}
};

TypedArray<Dictionary> WarningCaptureEditor::_scan_all_gdscripts() {
	TypedArray<Dictionary> all_warnings;

	// Get GDScriptLanguage singleton
	GDScriptLanguage *gdscript_lang = GDScriptLanguage::get_singleton();
	if (!gdscript_lang) {
		print_line("WarningCaptureEditor: GDScriptLanguage not available");
		return all_warnings;
	}

	// Find all .gd files in the project
	Vector<String> gdscript_files;
	String project_dir = ProjectSettings::get_singleton()->globalize_path("res://");
	_find_all_gdscript_files(project_dir, gdscript_files);

	// Sort files by priority: game code first, unit tests second, integration tests last
	// This ensures critical errors are found early and slow integration tests come last
	gdscript_files.sort_custom<_ValidationPriorityComparator>();

	print_line("WarningCaptureEditor: Scanning " + itos(gdscript_files.size()) + " GDScript files for warnings...");
	print_line("WarningCaptureEditor: Files sorted by priority: Core → Scripts → Tools → Unit Tests → Integration Tests");

	// Track current category for progress reporting
	int current_priority = -1;

	// Validate each file and collect warnings
	for (int i = 0; i < gdscript_files.size(); i++) {
		String file_path = gdscript_files[i];

		// Report when we enter a new priority category
		int file_priority = _ValidationPriorityComparator::_get_priority(file_path);
		if (file_priority != current_priority) {
			current_priority = file_priority;
			String category = "Other";
			if (file_priority == 0) category = "Core (autoload/core)";
			else if (file_priority == 1) category = "Game Scripts";
			else if (file_priority == 2) category = "Tools/Addons";
			else if (file_priority == 3) category = "Unit Tests";
			else if (file_priority == 4) category = "Integration Tests";
			print_line("WarningCaptureEditor: [" + category + "] Starting validation...");
		}

		// Progress reporting every 25 files
		if (i % 25 == 0 && i > 0) {
			print_line("WarningCaptureEditor: Progress: " + itos(i) + " / " + itos(gdscript_files.size()) + " files validated...");
		}

		// Convert to res:// path
		String res_path = ProjectSettings::get_singleton()->localize_path(file_path);

		// Read source code
		Ref<FileAccess> file = FileAccess::open(file_path, FileAccess::READ);
		if (file.is_null()) {
			continue;  // Silently skip files that can't be opened
		}
		String source_code = file->get_as_text();
		file->close();

		// Skip files that cause validation to hang (complex dependency chains)
		// Integration tests often have deep dependency trees that cause validate() to hang
		if (res_path.contains("test/integration/")) {
			Dictionary error_dict;
			error_dict["source_file"] = res_path;
			error_dict["source_line"] = 0;
			error_dict["message"] = "Skipped validation (integration test with complex dependencies)";
			error_dict["is_warning"] = true;
			error_dict["is_error"] = false;
			all_warnings.append(error_dict);
			continue;
		}

		// Validate the script
		List<String> functions;  // Not used, but required for validate() signature
		List<ScriptLanguage::ScriptError> errors;
		List<ScriptLanguage::Warning> warnings;

		gdscript_lang->validate(source_code, res_path, &functions, &errors, &warnings);

		// Convert warnings to Dictionary format
		for (const ScriptLanguage::Warning &w : warnings) {
			Dictionary warning_dict;
			warning_dict["source_file"] = res_path;
			warning_dict["source_line"] = w.start_line;
			warning_dict["message"] = w.message;
			warning_dict["code"] = w.string_code;  // SHADOWED_VARIABLE, UNUSED_VARIABLE, etc.
			warning_dict["is_warning"] = true;
			warning_dict["is_error"] = false;
			all_warnings.append(warning_dict);
		}

		// Also capture parse/compile errors from validation
		for (const ScriptLanguage::ScriptError &e : errors) {
			Dictionary error_dict;
			error_dict["source_file"] = e.path.is_empty() ? res_path : e.path;
			error_dict["source_line"] = e.line;
			error_dict["message"] = e.message;
			error_dict["is_warning"] = false;
			error_dict["is_error"] = true;
			all_warnings.append(error_dict);
		}
	}

	print_line("WarningCaptureEditor: Found " + itos(all_warnings.size()) + " warnings/errors from GDScript validation");
	return all_warnings;
}
