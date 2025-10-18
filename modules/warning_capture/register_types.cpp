#include "register_types.h"

#ifdef TOOLS_ENABLED
#include "editor/warning_capture_editor.h"
#endif

void initialize_warning_capture_module(ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		ClassDB::register_class<WarningCaptureEditor>();
	}
#endif
}

void uninitialize_warning_capture_module(ModuleInitializationLevel p_level) {
	// Nothing to do here
}