/**************************************************************************/
/*  script_validator.cpp                                                  */
/**************************************************************************/
/*                       This file is part of:                            */
/*                           GODOT ENGINE                                 */
/*                      https://godotengine.org                           */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md).*/
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "script_validator.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "core/object/script_language.h"

int ScriptValidator::validate_script(const String &p_path, bool p_json_output) {
	// Read the script file
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		if (p_json_output) {
			Dictionary result;
			result["path"] = p_path;
			result["valid"] = false;
			Array errors;
			Dictionary err;
			err["line"] = 0;
			err["column"] = 0;
			err["message"] = "Cannot open file: " + p_path;
			errors.push_back(err);
			result["errors"] = errors;
			result["warnings"] = Array();
			OS::get_singleton()->print("%s\n", JSON::stringify(result, "", false).utf8().get_data());
		} else {
			OS::get_singleton()->print("ERROR: Cannot open file: %s\n", p_path.utf8().get_data());
		}
		return EXIT_ERRORS;
	}

	String script_content = f->get_as_text();
	f->close();

	// Get the GDScript language
	ScriptLanguage *lang = ScriptServer::get_language_for_extension("gd");
	if (!lang) {
		OS::get_singleton()->print("ERROR: GDScript language not available.\n");
		return EXIT_ERRORS;
	}

	// Validate the script
	List<String> functions;
	List<ScriptLanguage::ScriptError> errors;
	List<ScriptLanguage::Warning> warnings;
	bool valid = lang->validate(script_content, p_path, &functions, &errors, &warnings, nullptr);

	if (p_json_output) {
		Dictionary result;
		result["path"] = p_path;
		result["valid"] = valid;

		Array errors_array;
		for (const ScriptLanguage::ScriptError &e : errors) {
			Dictionary err;
			err["line"] = e.line;
			err["column"] = e.column;
			err["message"] = e.message;
			if (!e.path.is_empty() && e.path != p_path) {
				err["path"] = e.path;
			}
			errors_array.push_back(err);
		}
		result["errors"] = errors_array;

		Array warnings_array;
		for (const ScriptLanguage::Warning &w : warnings) {
			Dictionary warn;
			warn["line"] = w.start_line;
			warn["column"] = w.leftmost_column;
			warn["end_line"] = w.end_line;
			warn["end_column"] = w.rightmost_column;
			warn["code"] = w.string_code;
			warn["message"] = w.message;
			warnings_array.push_back(warn);
		}
		result["warnings"] = warnings_array;

		OS::get_singleton()->print("%s\n", JSON::stringify(result, "", false).utf8().get_data());
	} else {
		// Text output
		if (valid && errors.is_empty() && warnings.is_empty()) {
			OS::get_singleton()->print("%s: OK\n", p_path.utf8().get_data());
		} else {
			for (const ScriptLanguage::ScriptError &e : errors) {
				OS::get_singleton()->print("%s:%d:%d: ERROR: %s\n",
						p_path.utf8().get_data(),
						e.line, e.column,
						e.message.utf8().get_data());
			}
			for (const ScriptLanguage::Warning &w : warnings) {
				OS::get_singleton()->print("%s:%d:%d: WARNING [%s]: %s\n",
						p_path.utf8().get_data(),
						w.start_line, w.leftmost_column,
						w.string_code.utf8().get_data(),
						w.message.utf8().get_data());
			}
		}
	}

	// Return exit code
	if (!errors.is_empty()) {
		return EXIT_ERRORS;
	} else if (!warnings.is_empty()) {
		return EXIT_WARNINGS_ONLY;
	}
	return EXIT_VALID;
}

int ScriptValidator::validate_directory(const String &p_path, bool p_json_output) {
	// Get the GDScript language
	ScriptLanguage *lang = ScriptServer::get_language_for_extension("gd");
	if (!lang) {
		OS::get_singleton()->print("ERROR: GDScript language not available.\n");
		return EXIT_ERRORS;
	}

	// Recursively find all .gd files
	Vector<String> script_files;
	Vector<String> dirs_to_scan;
	dirs_to_scan.push_back(p_path);

	while (!dirs_to_scan.is_empty()) {
		String current_dir = dirs_to_scan[dirs_to_scan.size() - 1];
		dirs_to_scan.remove_at(dirs_to_scan.size() - 1);

		Ref<DirAccess> da = DirAccess::open(current_dir);
		if (da.is_null()) {
			continue;
		}

		da->list_dir_begin();
		String file_name = da->get_next();
		while (!file_name.is_empty()) {
			if (file_name == "." || file_name == "..") {
				file_name = da->get_next();
				continue;
			}

			String full_path = current_dir.path_join(file_name);

			if (da->current_is_dir()) {
				// Skip .godot and .git directories
				if (file_name != ".godot" && file_name != ".git") {
					dirs_to_scan.push_back(full_path);
				}
			} else if (file_name.ends_with(".gd")) {
				script_files.push_back(full_path);
			}

			file_name = da->get_next();
		}
		da->list_dir_end();
	}

	// Validate all scripts
	int total_errors = 0;
	int total_warnings = 0;
	int files_with_errors = 0;
	int files_with_warnings = 0;
	Array all_issues; // For JSON output

	for (const String &script_path : script_files) {
		Ref<FileAccess> f = FileAccess::open(script_path, FileAccess::READ);
		if (f.is_null()) {
			if (p_json_output) {
				Dictionary issue;
				issue["file"] = script_path;
				issue["line"] = 0;
				issue["column"] = 0;
				issue["severity"] = "error";
				issue["message"] = "Cannot open file";
				all_issues.push_back(issue);
			} else {
				OS::get_singleton()->print("%s:0:0: ERROR: Cannot open file\n", script_path.utf8().get_data());
			}
			total_errors++;
			files_with_errors++;
			continue;
		}

		String script_content = f->get_as_text();
		f->close();

		List<String> functions;
		List<ScriptLanguage::ScriptError> errors;
		List<ScriptLanguage::Warning> warnings;
		lang->validate(script_content, script_path, &functions, &errors, &warnings, nullptr);

		if (!errors.is_empty()) {
			files_with_errors++;
		}
		if (!warnings.is_empty()) {
			files_with_warnings++;
		}

		for (const ScriptLanguage::ScriptError &e : errors) {
			total_errors++;
			if (p_json_output) {
				Dictionary issue;
				issue["file"] = script_path;
				issue["line"] = e.line;
				issue["column"] = e.column;
				issue["severity"] = "error";
				issue["message"] = e.message;
				all_issues.push_back(issue);
			} else {
				OS::get_singleton()->print("%s:%d:%d: ERROR: %s\n",
						script_path.utf8().get_data(),
						e.line, e.column,
						e.message.utf8().get_data());
			}
		}

		for (const ScriptLanguage::Warning &w : warnings) {
			total_warnings++;
			if (p_json_output) {
				Dictionary issue;
				issue["file"] = script_path;
				issue["line"] = w.start_line;
				issue["column"] = w.leftmost_column;
				issue["end_line"] = w.end_line;
				issue["end_column"] = w.rightmost_column;
				issue["severity"] = "warning";
				issue["code"] = w.string_code;
				issue["message"] = w.message;
				all_issues.push_back(issue);
			} else {
				OS::get_singleton()->print("%s:%d:%d: WARNING [%s]: %s\n",
						script_path.utf8().get_data(),
						w.start_line, w.leftmost_column,
						w.string_code.utf8().get_data(),
						w.message.utf8().get_data());
			}
		}
	}

	// Output summary
	if (p_json_output) {
		Dictionary result;
		result["project_path"] = p_path;
		result["files_checked"] = script_files.size();
		result["files_with_errors"] = files_with_errors;
		result["files_with_warnings"] = files_with_warnings;
		result["total_errors"] = total_errors;
		result["total_warnings"] = total_warnings;
		result["issues"] = all_issues;
		OS::get_singleton()->print("%s\n", JSON::stringify(result, "", false).utf8().get_data());
	} else {
		OS::get_singleton()->print("\nSummary: %d files checked, %d errors, %d warnings\n",
				(int)script_files.size(), total_errors, total_warnings);
	}

	// Return exit code
	if (total_errors > 0) {
		return EXIT_ERRORS;
	} else if (total_warnings > 0) {
		return EXIT_WARNINGS_ONLY;
	}
	return EXIT_VALID;
}
