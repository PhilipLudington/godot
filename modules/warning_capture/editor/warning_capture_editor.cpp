#include "warning_capture_editor.h"

#include "editor/editor_node.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/script_editor_debugger.h"
#include "core/io/file_access.h"
#include "core/io/json.h"

void WarningCaptureEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_all_warnings_and_errors"), &WarningCaptureEditor::get_all_warnings_and_errors);
	ClassDB::bind_method(D_METHOD("dump_errors_to_file", "file_path"), &WarningCaptureEditor::dump_errors_to_file);
	ClassDB::bind_method(D_METHOD("clear_errors"), &WarningCaptureEditor::clear_errors);
}

TypedArray<Dictionary> WarningCaptureEditor::get_all_warnings_and_errors() const {
	TypedArray<Dictionary> result;

	// Check if we're in the editor
	if (!EditorNode::get_singleton()) {
		return result;
	}

	// Get the debugger node
	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	if (!debugger_node) {
		return result;
	}

	// Get the default debugger
	ScriptEditorDebugger *default_debugger = debugger_node->get_default_debugger();
	if (!default_debugger) {
		return result;
	}

	// Get all errors from the debugger
	return default_debugger->get_all_errors();
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

	// Write as JSONL format (one JSON object per line)
	for (int i = 0; i < errors.size(); i++) {
		Dictionary error = errors[i];
		String json_line = JSON::stringify(error);
		file->store_line(json_line);
	}

	file->close();
	print_line("Dumped " + itos(errors.size()) + " errors/warnings to " + p_file_path);
}

void WarningCaptureEditor::clear_errors() {
	if (!EditorNode::get_singleton()) {
		return;
	}

	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	if (!debugger_node) {
		return;
	}

	ScriptEditorDebugger *default_debugger = debugger_node->get_default_debugger();
	if (!default_debugger) {
		return;
	}

	default_debugger->clear_errors();
}

WarningCaptureEditor::WarningCaptureEditor() {
}