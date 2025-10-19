#include "register_types.h"

#ifdef TOOLS_ENABLED
#include "editor/warning_capture_editor.h"
#include "editor/editor_node.h"
#include "scene/gui/button.h"
#include "core/error/error_macros.h"

// Singleton for warning capture
Ref<WarningCaptureEditor> _warning_capture_singleton;
static ErrorHandlerList _error_handler;

// Error handler callback that captures all errors
static void _warning_capture_error_handler(void *p_self, const char *p_function, const char *p_file, int p_line, const char *p_error, const char *p_errorexp, bool p_editor_notify, ErrorHandlerType p_type) {
	if (!_warning_capture_singleton.is_valid()) {
		return;
	}

	// Create a dictionary for this error
	Dictionary error_dict;
	error_dict["function"] = String(p_function);
	error_dict["file"] = String(p_file);
	error_dict["line"] = p_line;
	error_dict["message"] = String(p_error);
	error_dict["error_type"] = p_type;

	// Add to the warning capture
	_warning_capture_singleton->add_warning(error_dict);
}
#endif

void initialize_warning_capture_module(ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		ClassDB::register_class<WarningCaptureEditor>();

		// Auto-instantiate the warning capture editor
		_warning_capture_singleton = memnew(WarningCaptureEditor);

		// Register error handler to capture ALL errors from this point forward
		_error_handler.userdata = nullptr;
		_error_handler.errfunc = _warning_capture_error_handler;
		add_error_handler(&_error_handler);

		// Register as editor singleton so it's accessible from GDScript
		Engine::get_singleton()->add_singleton(Engine::Singleton("WarningCapture", _warning_capture_singleton.ptr()));

		print_line("==========================================");
		print_line("WARNING CAPTURE MODULE: Initialized");
		print_line("Access from console: WarningCapture.capture_debugger_warnings_now()");
		print_line("Warnings will be written to godot/diagnostics/warnings.json");
		print_line("==========================================");
	}
#endif
}

void uninitialize_warning_capture_module(ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		// Remove error handler
		remove_error_handler(&_error_handler);

		if (_warning_capture_singleton.is_valid()) {
			_warning_capture_singleton.unref();
		}
	}
#endif
}