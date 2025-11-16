#include "register_types.h"

#ifdef TOOLS_ENABLED
#include "editor/warning_capture_editor.h"
#include "editor/editor_node.h"
#include "scene/gui/button.h"
#include "core/error/error_macros.h"
#include "core/config/project_settings.h"

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

		// Register project settings
		GLOBAL_DEF("diagnostics/auto_capture_enabled", true);
		ProjectSettings::get_singleton()->set_custom_property_info(
			PropertyInfo(Variant::BOOL, "diagnostics/auto_capture_enabled"));

		GLOBAL_DEF("diagnostics/debounce_interval_ms", 2000);
		ProjectSettings::get_singleton()->set_custom_property_info(
			PropertyInfo(Variant::INT, "diagnostics/debounce_interval_ms",
			PROPERTY_HINT_RANGE, "500,10000,500"));

		GLOBAL_DEF("diagnostics/per_file_timeout_ms", 500);
		ProjectSettings::get_singleton()->set_custom_property_info(
			PropertyInfo(Variant::INT, "diagnostics/per_file_timeout_ms",
			PROPERTY_HINT_RANGE, "100,5000,100"));

		GLOBAL_DEF("diagnostics/total_scan_timeout_ms", 90000);
		ProjectSettings::get_singleton()->set_custom_property_info(
			PropertyInfo(Variant::INT, "diagnostics/total_scan_timeout_ms",
			PROPERTY_HINT_RANGE, "10000,300000,5000"));

		// Auto-instantiate the warning capture editor
		_warning_capture_singleton = memnew(WarningCaptureEditor);

		// Apply settings to singleton
		bool auto_enabled = ProjectSettings::get_singleton()->get_setting("diagnostics/auto_capture_enabled");
		_warning_capture_singleton->set_auto_capture_enabled(auto_enabled);

		int debounce = ProjectSettings::get_singleton()->get_setting("diagnostics/debounce_interval_ms");
		_warning_capture_singleton->set_debounce_interval(debounce);

		int per_file = ProjectSettings::get_singleton()->get_setting("diagnostics/per_file_timeout_ms");
		_warning_capture_singleton->set_per_file_timeout(per_file);

		// Register error handler to capture ALL errors from this point forward
		_error_handler.userdata = nullptr;
		_error_handler.errfunc = _warning_capture_error_handler;
		add_error_handler(&_error_handler);

		// Register as editor singleton so it's accessible from GDScript
		Engine::get_singleton()->add_singleton(Engine::Singleton("WarningCapture", _warning_capture_singleton.ptr()));

		print_line("==========================================");
		print_line("WARNING CAPTURE MODULE: Initialized");
		print_line("Auto-capture: " + String(auto_enabled ? "ENABLED" : "DISABLED"));
		print_line("Access from console: WarningCapture.capture_debugger_warnings_now()");
		print_line("Warnings will be written to diagnostics/warnings.json");
		print_line("Debugger errors will be written to diagnostics/debugger.json");
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