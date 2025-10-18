#ifndef WARNING_CAPTURE_EDITOR_H
#define WARNING_CAPTURE_EDITOR_H

#include "core/object/ref_counted.h"
#include "core/variant/typed_array.h"

class WarningCaptureEditor : public RefCounted {
	GDCLASS(WarningCaptureEditor, RefCounted);

protected:
	static void _bind_methods();

public:
	TypedArray<Dictionary> get_all_warnings_and_errors() const;
	void dump_errors_to_file(const String &p_file_path) const;
	void clear_errors();

	WarningCaptureEditor();
};

#endif // WARNING_CAPTURE_EDITOR_H