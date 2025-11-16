#include "warning_capture_editor.h"

#include "editor/editor_node.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor/editor_log.h"
#include "editor/editor_file_system.h"
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
	ClassDB::bind_method(D_METHOD("capture_debugger_errors_now"), &WarningCaptureEditor::capture_debugger_errors_now);

	// NEW bindings for configuration
	ClassDB::bind_method(D_METHOD("set_auto_capture_enabled", "enabled"), &WarningCaptureEditor::set_auto_capture_enabled);
	ClassDB::bind_method(D_METHOD("set_debounce_interval", "milliseconds"), &WarningCaptureEditor::set_debounce_interval);
	ClassDB::bind_method(D_METHOD("set_per_file_timeout", "milliseconds"), &WarningCaptureEditor::set_per_file_timeout);

	// Bind internal methods that need to be called via call_deferred
	ClassDB::bind_method(D_METHOD("_setup_event_listeners"), &WarningCaptureEditor::_setup_event_listeners);
}

WarningCaptureEditor::WarningCaptureEditor() {
	// Initialize file paths - all go to diagnostics/
	String project_dir = ProjectSettings::get_singleton()->globalize_path("res://");
	String diagnostics_dir = project_dir.path_join("diagnostics");
	warnings_file_path = diagnostics_dir.path_join("warnings.json");
	debugger_file_path = diagnostics_dir.path_join("debugger.json");
	timestamp_file_path = diagnostics_dir.path_join(".last_updated");
	metadata_file_path = diagnostics_dir.path_join(".scan_metadata.json");

	// Ensure diagnostics directory exists
	DirAccess::make_dir_recursive_absolute(diagnostics_dir);

	// Clear the warnings file on startup (fresh session)
	Ref<FileAccess> file = FileAccess::open(warnings_file_path, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string("{}");
		file->close();
	}

	// Clear the debugger file on startup (fresh session)
	Ref<FileAccess> debugger_file = FileAccess::open(debugger_file_path, FileAccess::WRITE);
	if (debugger_file.is_valid()) {
		debugger_file->store_string("{}");
		debugger_file->close();
	}

	// Setup event listeners (deferred until scene tree ready)
	call_deferred("_setup_event_listeners");
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

// Event listener setup
void WarningCaptureEditor::_setup_event_listeners() {
	if (!EditorNode::get_singleton()) {
		print_line("WarningCaptureEditor: EditorNode not available, event listeners not set up");
		return;
	}

	print_line("WarningCaptureEditor: Setting up auto-capture event listeners...");

	// Listen for filesystem changes (file saves, imports, deletions)
	EditorFileSystem *efs = EditorFileSystem::get_singleton();
	if (efs) {
		efs->connect("filesystem_changed", callable_mp(this, &WarningCaptureEditor::_on_filesystem_changed));
		print_line("WarningCaptureEditor: Connected to filesystem_changed signal");
	} else {
		print_line("WarningCaptureEditor: WARNING - EditorFileSystem not available");
	}

	print_line("WarningCaptureEditor: Auto-capture enabled - will scan on file saves");
}

// Debouncing logic
bool WarningCaptureEditor::_should_skip_scan() {
	if (!auto_capture_enabled) {
		return true;
	}

	uint64_t now = Time::get_singleton()->get_ticks_msec();
	uint64_t time_since_last = now - last_scan_timestamp;

	if (time_since_last < debounce_interval_ms) {
		print_line("WarningCaptureEditor: Skipping scan (debounce: " + itos(time_since_last) + "ms < " + itos(debounce_interval_ms) + "ms)");
		return true;
	}

	return false;
}

// Event handlers
void WarningCaptureEditor::_on_filesystem_changed() {
	if (_should_skip_scan()) {
		return;
	}

	print_line("WarningCaptureEditor: Filesystem changed, auto-capturing diagnostics...");
	uint64_t start_time = Time::get_singleton()->get_ticks_msec();

	// Capture all diagnostics
	capture_debugger_warnings_now();
	capture_debugger_errors_now();

	uint64_t duration = Time::get_singleton()->get_ticks_msec() - start_time;
	last_scan_timestamp = Time::get_singleton()->get_ticks_msec();

	// Write timestamp file for Claude Code
	_write_timestamp_file();

	print_line("WarningCaptureEditor: Auto-capture completed in " + itos(duration) + "ms");
}

void WarningCaptureEditor::_on_script_saved(Ref<Script> p_script) {
	// For now, just trigger full scan
	// Future optimization: incremental scan for just this script
	_on_filesystem_changed();
}

// Timestamp file writer
void WarningCaptureEditor::_write_timestamp_file() {
	Ref<FileAccess> file = FileAccess::open(timestamp_file_path, FileAccess::WRITE);
	if (file.is_valid()) {
		Dictionary timestamp_data;
		timestamp_data["timestamp_ms"] = Time::get_singleton()->get_ticks_msec();
		timestamp_data["datetime"] = Time::get_singleton()->get_datetime_string_from_system();
		timestamp_data["warnings_file"] = "diagnostics/warnings.json";
		timestamp_data["debugger_file"] = "diagnostics/debugger.json";
		timestamp_data["metadata_file"] = "diagnostics/.scan_metadata.json";

		file->store_string(JSON::stringify(timestamp_data, "\t"));
		file->close();
	}
}

// Metadata file writer
void WarningCaptureEditor::_write_metadata_file(uint64_t p_duration_ms, int p_files_scanned, int p_files_skipped) {
	Ref<FileAccess> file = FileAccess::open(metadata_file_path, FileAccess::WRITE);
	if (file.is_valid()) {
		Dictionary metadata;
		metadata["scan_duration_ms"] = p_duration_ms;
		metadata["total_files_scanned"] = p_files_scanned;
		metadata["files_skipped_timeout"] = p_files_skipped;
		metadata["scan_timestamp"] = Time::get_singleton()->get_datetime_string_from_system();
		metadata["within_budget"] = p_duration_ms <= total_scan_timeout_ms;

		String performance_rating = "slow";
		if (p_duration_ms < 30000) {
			performance_rating = "excellent";
		} else if (p_duration_ms < 60000) {
			performance_rating = "good";
		}
		metadata["performance_rating"] = performance_rating;

		file->store_string(JSON::stringify(metadata, "\t"));
		file->close();
	}
}

// Configuration methods
void WarningCaptureEditor::set_auto_capture_enabled(bool p_enabled) {
	auto_capture_enabled = p_enabled;
	print_line("WarningCaptureEditor: Auto-capture " + String(p_enabled ? "enabled" : "disabled"));
}

void WarningCaptureEditor::set_debounce_interval(uint64_t p_ms) {
	debounce_interval_ms = p_ms;
}

void WarningCaptureEditor::set_per_file_timeout(uint64_t p_ms) {
	per_file_timeout_ms = p_ms;
}

// Estimate fix time helper
String WarningCaptureEditor::_estimate_fix_time(int p_issue_count) const {
	if (p_issue_count == 0) return "0 minutes";
	if (p_issue_count <= 5) return "2-5 minutes";
	if (p_issue_count <= 20) return "5-15 minutes";
	if (p_issue_count <= 50) return "15-30 minutes";
	return "30+ minutes";
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

	// Count by category and file
	Dictionary by_severity;
	Dictionary by_source;
	Dictionary by_category;
	Dictionary by_file;

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

		// NEW: Count per file
		String file = warning.get("file", "unknown");
		Variant current_file = by_file.get(file, 0);
		int file_count = (int)current_file;
		by_file[file] = file_count + 1;
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
		fix_sequence.append(vformat("Fix %d parse/syntax error(s) - blocks compilation", error_count));
	}
	if (warning_count > 0) {
		fix_sequence.append(vformat("Fix %d compiler warning(s) - improves code quality", warning_count));
	}

	// NEW: Determine recommended starting file (file with most issues)
	String start_file = "";
	int max_issues = 0;
	Array file_keys = by_file.keys();
	for (int i = 0; i < file_keys.size(); i++) {
		String file = file_keys[i];
		int count = (int)by_file.get(file, 0);
		if (count > max_issues) {
			max_issues = count;
			start_file = file;
		}
	}

	// NEW: Identify batch-fixable categories
	Array batch_fixable;
	if ((int)by_category.get("unused_variable", 0) > 0) {
		batch_fixable.append("unused_variable");
	}
	if ((int)by_category.get("shadowed_variable", 0) > 0) {
		batch_fixable.append("shadowed_variable");
	}

	// Build metadata
	Dictionary metadata;
	metadata["generated_at"] = Time::get_singleton()->get_datetime_string_from_system();
	metadata["project"] = ProjectSettings::get_singleton()->get_setting("application/config/name", "Unknown Project");
	metadata["total_issues"] = sorted_warnings.size();
	metadata["by_source"] = by_source;
	metadata["by_severity"] = by_severity;
	metadata["by_category"] = by_category;
	metadata["by_file"] = by_file;
	metadata["fix_ready"] = true;
	metadata["schema_version"] = "1.0";

	// NEW: Claude Code hints
	Dictionary claude_hints;
	claude_hints["start_with_file"] = start_file;
	claude_hints["max_issues_in_file"] = max_issues;
	claude_hints["batch_fixes_available"] = batch_fixable;
	claude_hints["estimated_fix_time"] = _estimate_fix_time(sorted_warnings.size());
	claude_hints["priority_order"] = "errors_first";

	// Build final report
	Dictionary report;
	report["metadata"] = metadata;
	report["claude_hints"] = claude_hints;
	report["issues"] = sorted_warnings;
	report["fix_sequence"] = fix_sequence;

	// Write to file
	Ref<FileAccess> file = FileAccess::open(warnings_file_path, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(JSON::stringify(report, "\t"));  // Pretty print with tabs
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
	int files_scanned = 0;
	int files_skipped_timeout = 0;
	uint64_t total_start_time = Time::get_singleton()->get_ticks_msec();

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

	print_line("WarningCaptureEditor: Scanning " + itos(gdscript_files.size()) + " GDScript files (timeout: " + itos(per_file_timeout_ms) + "ms per file)...");
	print_line("WarningCaptureEditor: Files sorted by priority: Core → Scripts → Tools → Unit Tests → Integration Tests");

	// Track current category for progress reporting
	int current_priority = -1;

	// Validate each file and collect warnings
	for (int i = 0; i < gdscript_files.size(); i++) {
		// Check total budget
		uint64_t elapsed = Time::get_singleton()->get_ticks_msec() - total_start_time;
		if (elapsed > total_scan_timeout_ms) {
			print_line("WarningCaptureEditor: TIMEOUT - Total scan budget exceeded (" + itos(elapsed) + "ms > " + itos(total_scan_timeout_ms) + "ms)");
			print_line("WarningCaptureEditor: Scanned " + itos(files_scanned) + " / " + itos(gdscript_files.size()) + " files before timeout");
			break;
		}

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
			print_line("WarningCaptureEditor: Progress: " + itos(i) + " / " + itos(gdscript_files.size()) + " files...");
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

		// Validate the script with timeout tracking
		List<String> functions;  // Not used, but required for validate() signature
		List<ScriptLanguage::ScriptError> errors;
		List<ScriptLanguage::Warning> warnings;

		uint64_t file_start_time = Time::get_singleton()->get_ticks_msec();

		// IMPORTANT: GDScriptLanguage::validate() is synchronous and can hang
		// We check the duration AFTER it completes
		gdscript_lang->validate(source_code, res_path, &functions, &errors, &warnings);

		uint64_t file_duration = Time::get_singleton()->get_ticks_msec() - file_start_time;

		// Log slow validations
		if (file_duration > per_file_timeout_ms) {
			print_line("WarningCaptureEditor: SLOW validation: " + res_path + " took " + itos(file_duration) + "ms (threshold: " + itos(per_file_timeout_ms) + "ms)");
			files_skipped_timeout++;

			// Still capture partial results if available
			Dictionary timeout_warning;
			timeout_warning["source_file"] = res_path;
			timeout_warning["source_line"] = 0;
			timeout_warning["message"] = "File validation exceeded timeout (" + itos(file_duration) + "ms > " + itos(per_file_timeout_ms) + "ms) - results may be incomplete";
			timeout_warning["code"] = "VALIDATION_TIMEOUT";
			timeout_warning["is_warning"] = true;
			timeout_warning["is_error"] = false;
			all_warnings.append(timeout_warning);
		}

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

		files_scanned++;
	}

	uint64_t total_duration = Time::get_singleton()->get_ticks_msec() - total_start_time;
	print_line("WarningCaptureEditor: Scan complete - " + itos(files_scanned) + " files in " + itos(total_duration) + "ms");
	print_line("WarningCaptureEditor: Found " + itos(all_warnings.size()) + " issues (" + itos(files_skipped_timeout) + " slow files)");

	// Write metadata
	_write_metadata_file(total_duration, files_scanned, files_skipped_timeout);

	return all_warnings;
}

// NEW: Capture debugger errors to debugger.json (separate from warnings)
void WarningCaptureEditor::capture_debugger_errors_now() {
	print_line("WarningCaptureEditor: capture_debugger_errors_now() called");
	accumulated_debugger_errors.clear();

	// Get errors from the debugger (runtime errors, not compile warnings)
	print_line("WarningCaptureEditor: Getting debugger errors...");
	TypedArray<Dictionary> debugger_errors = _get_debugger_errors();
	print_line("WarningCaptureEditor: Found " + itos(debugger_errors.size()) + " debugger errors");

	for (int i = 0; i < debugger_errors.size(); i++) {
		Dictionary categorized = _categorize_error(debugger_errors[i]);
		accumulated_debugger_errors.append(categorized);
	}

	// Write to debugger.json
	print_line("WarningCaptureEditor: Writing debugger file...");
	_write_debugger_file();
	print_line("WarningCaptureEditor: COMPLETE! Captured " + itos(accumulated_debugger_errors.size()) + " total debugger errors");
}

TypedArray<Dictionary> WarningCaptureEditor::_get_debugger_errors() const {
	// Get ALL errors from ScriptEditorDebugger (runtime errors from the Debugger-Errors tab)
	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	if (!debugger_node) {
		return TypedArray<Dictionary>();
	}

	ScriptEditorDebugger *default_debugger = debugger_node->get_default_debugger();
	if (!default_debugger) {
		return TypedArray<Dictionary>();
	}

	// Return ALL errors from debugger
	return default_debugger->get_all_errors();
}

void WarningCaptureEditor::_write_debugger_file() {
	if (debugger_file_path.is_empty()) {
		return;
	}

	// Sort errors by severity then file/line
	TypedArray<Dictionary> sorted_errors = accumulated_debugger_errors;

	// Count by category
	Dictionary by_severity;
	Dictionary by_source;
	Dictionary by_category;

	for (int i = 0; i < sorted_errors.size(); i++) {
		Dictionary error = sorted_errors[i];

		String severity = error.get("severity", "unknown");
		Variant current_sev = by_severity.get(severity, 0);
		int sev_count = (int)current_sev;
		by_severity[severity] = sev_count + 1;

		String source = error.get("source", "unknown");
		Variant current_src = by_source.get(source, 0);
		int src_count = (int)current_src;
		by_source[source] = src_count + 1;

		String category = error.get("category", "unknown");
		Variant current_cat = by_category.get(category, 0);
		int cat_count = (int)current_cat;
		by_category[category] = cat_count + 1;
	}

	// Add priorities
	for (int i = 0; i < sorted_errors.size(); i++) {
		Dictionary error = sorted_errors[i];
		error["priority"] = i + 1;
		sorted_errors[i] = error;
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
	metadata["total_issues"] = sorted_errors.size();
	metadata["by_source"] = by_source;
	metadata["by_severity"] = by_severity;
	metadata["by_category"] = by_category;

	Dictionary report;
	report["metadata"] = metadata;
	report["issues"] = sorted_errors;
	report["fix_sequence"] = fix_sequence;

	// Write to debugger.json file
	Ref<FileAccess> file = FileAccess::open(debugger_file_path, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(JSON::stringify(report));
		file->close();
	}
}
