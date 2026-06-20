export module ObjectDataText;

import std;
import std.compat;
import SLK;
import Utilities;
import UnorderedMap;
import TriggerStrings;

namespace fs = std::filesystem;

export struct ObjectTextExportOptions {
	bool w3x2lni_format = false;
	const TriggerStrings* trigger_strings = nullptr;
};

export struct TextParseIssue {
	int line = 0;
	std::string message;
};

export struct ParsedTextObject {
	std::string id;
	hive::unordered_map<std::string, std::string> fields;
};

export struct ParsedTextTable {
	std::vector<ParsedTextObject> objects;
	std::vector<TextParseIssue> issues;
};

namespace {

std::string trim_copy(std::string_view value) {
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
		value.remove_prefix(1);
	}
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
		value.remove_suffix(1);
	}
	return std::string(value);
}

std::string unquote_copy(std::string_view value) {
	std::string trimmed = trim_copy(value);
	if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
		const std::string_view inner = std::string_view(trimmed).substr(1, trimmed.size() - 2);
		std::string unescaped;
		unescaped.reserve(inner.size());
		for (size_t i = 0; i < inner.size(); ++i) {
			if (inner[i] == '\\' && i + 1 < inner.size()) {
				unescaped.push_back(inner[i + 1]);
				++i;
			} else {
				unescaped.push_back(inner[i]);
			}
		}
		return unescaped;
	}
	return trimmed;
}

std::optional<size_t> find_quoted_string_end(const std::string_view text, const size_t start) {
	if (start >= text.size() || text[start] != '"') {
		return std::nullopt;
	}

	for (size_t i = start + 1; i < text.size(); ++i) {
		if (text[i] == '\\' && i + 1 < text.size()) {
			++i;
			continue;
		}
		if (text[i] == '"') {
			return i;
		}
	}
	return std::nullopt;
}

// Parse the value portion (everything after '=') of one INI property line.
// Strips the trailing HiveWE export comment (" ; Field (type)") and returns the
// unescaped string value.  Handles both well-formed lines:
//   field = "some \"value\"" ; Comment (string)
// and malformed lines where the closing '"' was missing in old exports:
//   field = "some value\" ; Comment (string)
std::string parse_property_value(std::string_view after_equals) {
	// Skip leading whitespace
	while (!after_equals.empty() && std::isspace(static_cast<unsigned char>(after_equals.front()))) {
		after_equals.remove_prefix(1);
	}

	std::string_view inner;

	if (!after_equals.empty() && after_equals.front() == '"') {
		// Quoted value — find the real closing '"' (not one preceded by '\')
		if (const std::optional<size_t> end_quote = find_quoted_string_end(after_equals, 0)) {
			// Well-formed: take exactly what's between the quotes, ignore trailing comment
			inner = after_equals.substr(1, *end_quote - 1);
		} else {
			// Malformed: no proper closing '"' found.
			// Strip the HiveWE comment (" ; ...") from the end, then take
			// everything after the opening '"' as the value body.
			std::string_view body = after_equals;
			if (const size_t sep = body.rfind(" ; "); sep != std::string_view::npos) {
				body = body.substr(0, sep);
			} else if (const size_t sep2 = body.rfind(" ;"); sep2 != std::string_view::npos) {
				body = body.substr(0, sep2);
			}
			while (!body.empty() && std::isspace(static_cast<unsigned char>(body.back()))) {
				body.remove_suffix(1);
			}
			inner = (body.size() > 1) ? body.substr(1) : std::string_view {};
		}
	} else {
		// Unquoted value — strip comment and trailing whitespace
		if (const size_t sep = after_equals.rfind(" ; "); sep != std::string_view::npos) {
			after_equals = after_equals.substr(0, sep);
		} else if (const size_t sep2 = after_equals.rfind(" ;"); sep2 != std::string_view::npos) {
			after_equals = after_equals.substr(0, sep2);
		}
		while (!after_equals.empty() && std::isspace(static_cast<unsigned char>(after_equals.back()))) {
			after_equals.remove_suffix(1);
		}
		return std::string(after_equals);
	}

	// Unescape: '\x' → 'x'  (handles '\"' → '"', '\\' → '\')
	std::string result;
	result.reserve(inner.size());
	for (size_t i = 0; i < inner.size(); ++i) {
		if (inner[i] == '\\' && i + 1 < inner.size()) {
			result.push_back(inner[i + 1]);
			++i;
		} else {
			result.push_back(inner[i]);
		}
	}
	return result;
}

// For detecting multiline continuation: check if the value after '=' starts a
// quoted string that has no closing '"' even after comment stripping.
std::string_view strip_export_comment(std::string_view value) {
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
		value.remove_prefix(1);
	}
	if (!value.empty() && value.front() == '"') {
		if (const std::optional<size_t> end_quote = find_quoted_string_end(value, 0)) {
			return value.substr(0, *end_quote + 1);
		}
		// Malformed: strip comment from end as fallback
		if (const size_t sep = value.rfind(" ; "); sep != std::string_view::npos) {
			value = value.substr(0, sep);
		} else if (const size_t sep2 = value.rfind(" ;"); sep2 != std::string_view::npos) {
			value = value.substr(0, sep2);
		}
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
			value.remove_suffix(1);
		}
		return value;
	}
	if (const size_t sep = value.rfind(" ; "); sep != std::string_view::npos) {
		value = value.substr(0, sep);
	} else if (const size_t sep2 = value.rfind(" ;"); sep2 != std::string_view::npos) {
		value = value.substr(0, sep2);
	}
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
		value.remove_suffix(1);
	}
	return value;
}

std::string_view property_value_view(const std::string_view value_view) {
	return strip_export_comment(value_view);
}

bool looks_like_property_line(std::string_view line) {
	while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) {
		line.remove_prefix(1);
	}

	if (line.empty() || line.front() == '[' || line.starts_with(';') || line.starts_with("//")) {
		return false;
	}

	const size_t equals = line.find('=');
	if (equals == std::string_view::npos || equals == 0) {
		return false;
	}

	const std::string_view key = line.substr(0, equals);
	for (const char ch : key) {
		if (std::isspace(static_cast<unsigned char>(ch))) {
			return false;
		}
	}
	return true;
}

std::string sanitize_export_comment(std::string comment) {
	for (char& ch : comment) {
		if (ch == '\r' || ch == '\n' || ch == '\t') {
			ch = ' ';
		}
	}
	return comment;
}

std::string escape_ini_content(std::string_view value) {
	std::string escaped;
	escaped.reserve(value.size());
	for (const char ch : value) {
		if (ch == '\r') {
			continue;
		}
		if (ch == '\n') {
			escaped += "|n";
			continue;
		}
		if (ch == '"' || ch == '\\') {
			escaped.push_back('\\');
		}
		escaped.push_back(ch);
	}
	return escaped;
}

std::string quote_ini_value(const std::string_view value) {
	std::string quoted;
	quoted.reserve(value.size() + 2);
	quoted.push_back('"');
	quoted += escape_ini_content(value);
	quoted.push_back('"');
	return quoted;
}

std::string escape_ini_value(const std::string_view value) {
	return quote_ini_value(value);
}

void append_ini_property_line(std::string& output, const std::string_view key, const std::string_view value, const std::string_view comment = {}) {
	output += key;
	output += " = ";
	output += quote_ini_value(value);
	if (!comment.empty()) {
		output += " ; ";
		output += comment;
	}
	output += '\n';
}

// Returns true only when a line has a quoted value that genuinely spans
// multiple physical lines (i.e. the opening '"' has no closing '"' even after
// the HiveWE export comment has been stripped and the malformed-export fallback
// has been applied).  With well-formed or comment-stripped lines this will
// almost always return false, so multiline merging becomes a last resort.
bool property_has_unclosed_quote(std::string_view line) {
	const size_t equals = line.find('=');
	if (equals == std::string_view::npos || line.front() == '[') {
		return false;
	}

	// strip_export_comment already handles both the well-formed and the
	// malformed-export (missing closing '"') cases.
	std::string_view value_view = strip_export_comment(line.substr(equals + 1));
	while (!value_view.empty() && std::isspace(static_cast<unsigned char>(value_view.front()))) {
		value_view.remove_prefix(1);
	}

	if (value_view.empty() || value_view.front() != '"') {
		return false;
	}

	// Only true if there is genuinely no closing '"' after comment stripping
	// AND no ' ; ' separator was found (i.e. we're in a real multiline value).
	return !find_quoted_string_end(value_view, 0).has_value();
}

bool is_valid_object_id(const std::string_view id) {
	if (id.size() != 4) {
		return false;
	}

	for (const char ch : id) {
		if (!std::isalnum(static_cast<unsigned char>(ch))) {
			return false;
		}
	}
	return true;
}

bool is_string_like_field_type(const std::string_view type) {
	return type == "string" || type == "stringList" || type == "hotkey";
}

std::string normalize_exported_string(std::string value) {
	std::string normalized;
	normalized.reserve(value.size());
	for (size_t i = 0; i < value.size(); ++i) {
		if (value[i] == '\r') {
			if (i + 1 < value.size() && value[i + 1] == '\n') {
				++i;
			}
			normalized += "|n";
		} else if (value[i] == '\n') {
			normalized += "|n";
		} else {
			normalized.push_back(value[i]);
		}
	}
	return normalized;
}

std::string export_field_value(const std::string_view raw_value, const std::string_view type, const TriggerStrings* trigger_strings) {
	std::string value(raw_value);
	if (is_string_like_field_type(type) || type.empty()) {
		if (value.starts_with("TRIGSTR") && trigger_strings) {
			const std::string_view resolved = trigger_strings->string(value);
			if (!resolved.empty()) {
				value = std::string(resolved);
			}
		}
		value = normalize_exported_string(std::move(value));
	}
	return value;
}

std::string humanize_meta_type(const std::string_view type) {
	if (type == "string") {
		return "string";
	}
	if (type == "stringList") {
		return "string list";
	}
	if (type == "int") {
		return "int";
	}
	if (type == "bool") {
		return "bool";
	}
	if (type == "real" || type == "unreal") {
		return "real";
	}
	if (type == "hotkey") {
		return "hotkey";
	}
	if (type == "icon") {
		return "icon";
	}
	if (type == "model") {
		return "model";
	}
	if (type == "unitList") {
		return "unit list";
	}
	if (type == "abilityList" || type == "abilitySkinList" || type == "heroAbilityList") {
		return "ability list";
	}
	if (type == "buffList") {
		return "buff list";
	}
	if (type == "upgradeList") {
		return "upgrade list";
	}
	if (type == "techList") {
		return "tech list";
	}
	if (type == "targetList") {
		return "target list";
	}
	if (type.ends_with("Flags")) {
		return "flags";
	}
	return std::string(type);
}

std::string format_field_comment(const slk::SLK& meta_slk, const std::string_view meta_id) {
	const std::string_view display = meta_slk.data<std::string_view>("displayname", meta_id);
	const std::string_view type = meta_slk.data<std::string_view>("type", meta_id);

	const std::string type_label = type.empty() ? std::string {} : humanize_meta_type(type);
	const std::string display_label(display);

	if (!display_label.empty() && !type_label.empty()) {
		return sanitize_export_comment(std::format("{} ({})", display_label, type_label));
	}
	if (!display_label.empty()) {
		return sanitize_export_comment(display_label);
	}
	return sanitize_export_comment(type_label);
}

} // namespace

export ParsedTextTable parse_object_text_content(std::string_view contents, const bool w3x2lni_format) {
	ParsedTextTable table;

	if (contents.starts_with(std::string{ static_cast<char>(0xEF), static_cast<char>(0xBB), static_cast<char>(0xBF) })) {
		contents.remove_prefix(3);
	}

	int line_number = 1;
	ParsedTextObject* current = nullptr;
	hive::unordered_map<std::string, size_t> section_index;

	auto push_line = [&](std::string_view line, const int error_line) {
		while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) {
			line.remove_suffix(1);
		}

		if (line.empty() || line.starts_with(';') || line.starts_with("//")) {
			return;
		}

		if (line.front() == '[') {
			const size_t closing = line.find(']');
			if (closing == std::string_view::npos || closing <= 1) {
				table.issues.push_back({ error_line, "Malformed section header." });
				current = nullptr;
				return;
			}

			const std::string_view raw_section_id = line.substr(1, closing - 1);
			std::string section_id = unquote_copy(raw_section_id);
			if (!is_valid_object_id(section_id)) {
				table.issues.push_back({ error_line, std::format("Invalid object ID '{}'.", section_id) });
				current = nullptr;
				return;
			}

			if (section_index.contains(section_id)) {
				table.issues.push_back({ error_line, std::format("Duplicate section '{}'.", section_id) });
				current = &table.objects.at(section_index.at(section_id));
				return;
			}

			section_index.emplace(section_id, table.objects.size());
			table.objects.push_back({ section_id, {} });
			current = &table.objects.back();
			return;
		}

		if (!current) {
			table.issues.push_back({ error_line, "Property line found outside of a section." });
			return;
		}

		const size_t equals = line.find('=');
		if (equals == std::string_view::npos) {
			table.issues.push_back({ error_line, "Malformed property line." });
			return;
		}

		std::string key = trim_copy(line.substr(0, equals));
		std::string value = parse_property_value(line.substr(equals + 1));
		value = normalize_exported_string(std::move(value));

		if (key.empty()) {
			table.issues.push_back({ error_line, "Property key is empty." });
			return;
		}

		if (w3x2lni_format && key == "_parent") {
			key = "oldid";
		}

		current->fields[key] = std::move(value);
	};

	while (!contents.empty()) {
		const int record_line = line_number;

		const size_t eol = contents.find('\n');
		std::string_view line = (eol == std::string_view::npos) ? contents : contents.substr(0, eol);
		if (!line.empty() && line.back() == '\r') {
			line.remove_suffix(1);
		}

		std::string merged(line);
		if (eol == std::string_view::npos) {
			contents.remove_prefix(contents.size());
		} else {
			contents.remove_prefix(eol + 1);
			line_number++;
		}

		while (property_has_unclosed_quote(merged) && !contents.empty()) {
			const size_t next_eol = contents.find('\n');
			std::string_view next_line = (next_eol == std::string_view::npos) ? contents : contents.substr(0, next_eol);
			if (!next_line.empty() && next_line.back() == '\r') {
				next_line.remove_suffix(1);
			}

			if (looks_like_property_line(next_line)) {
				break;
			}

			merged.push_back('\n');
			merged.append(next_line);

			if (next_eol == std::string_view::npos) {
				contents.remove_prefix(contents.size());
			} else {
				contents.remove_prefix(next_eol + 1);
				line_number++;
			}
		}

		push_line(merged, record_line);
	}

	return table;
}

export ParsedTextTable parse_object_text_table(const fs::path& path, const bool w3x2lni_format) {
	ParsedTextTable table;
	const auto file = read_file(path);
	if (!file) {
		table.issues.push_back({ 0, file.error() });
		return table;
	}

	const std::string_view view(reinterpret_cast<const char*>(file->buffer.data()), file->buffer.size());
	return parse_object_text_content(view, w3x2lni_format);
}

export void apply_parsed_text_table(slk::SLK& slk, const ParsedTextTable& table) {
	for (const ParsedTextObject& object : table.objects) {
		const bool is_custom = object.fields.contains("oldid");
		if (is_custom && !slk.row_headers.contains(object.id)) {
			slk.copy_row(object.fields.at("oldid"), object.id, false);
		}

		for (const auto& [field, value] : object.fields) {
			slk.set_shadow_data(field, object.id, value);
		}
	}
}

export std::string build_object_text_table(const slk::SLK& slk, const slk::SLK& meta_slk, const ObjectTextExportOptions& options) {
	std::string output;
	std::vector<std::string> ids;
	ids.reserve(slk.shadow_data.size());
	for (const auto& [id, properties] : slk.shadow_data) {
		(void)properties;
		ids.push_back(id);
	}
	std::ranges::sort(ids);

	for (const std::string& id : ids) {
		const auto& properties = slk.shadow_data.at(id);
		output += std::format("[{}]\n", id);

		if (properties.contains("oldid")) {
			if (options.w3x2lni_format) {
				append_ini_property_line(output, "_parent", properties.at("oldid"));
			} else {
				append_ini_property_line(output, "oldid", properties.at("oldid"));
			}
		}

		std::vector<std::string> fields;
		fields.reserve(properties.size());
		for (const auto& [field, value] : properties) {
			(void)value;
			if (field == "oldid") {
				continue;
			}
			fields.push_back(field);
		}
		std::ranges::sort(fields);

		for (const std::string& field : fields) {
			const auto meta_id = slk.field_to_meta_id(meta_slk, field, id);
			const std::string_view type = meta_id ? meta_slk.data<std::string_view>("type", *meta_id) : std::string_view {};
			const std::string exported_value = export_field_value(properties.at(field), type, options.trigger_strings);
			if (meta_id && meta_slk.row_headers.contains(*meta_id)) {
				const std::string field_comment = format_field_comment(meta_slk, *meta_id);
				if (!field_comment.empty()) {
					append_ini_property_line(output, field, exported_value, field_comment);
					continue;
				}
			}
			append_ini_property_line(output, field, exported_value);
		}

		output += '\n';
	}

	return output;
}

export std::string build_object_text_table(const slk::SLK& slk, const slk::SLK& meta_slk, const bool w3x2lni_format) {
	return build_object_text_table(slk, meta_slk, ObjectTextExportOptions{ .w3x2lni_format = w3x2lni_format });
}

export bool import_value_matches_current(
	const std::string& imported_value,
	const std::string_view current_value,
	const std::string_view field_type,
	const TriggerStrings* trigger_strings
) {
	if (imported_value == current_value) {
		return true;
	}
	return imported_value == export_field_value(current_value, field_type, trigger_strings);
}

export std::expected<void, std::string> write_text_file(const fs::path& path, const std::string& contents) {
	std::error_code ec;
	fs::create_directories(path.parent_path(), ec);

	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream.is_open()) {
		return std::unexpected("Unable to write file: " + path.string());
	}

	stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
	if (!stream) {
		return std::unexpected("Error writing file: " + path.string());
	}

	return {};
}
