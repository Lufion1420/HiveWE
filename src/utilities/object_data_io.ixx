module;

#include <nlohmann/json.hpp>

export module ObjectDataIo;

import std;
import std.compat;
import types;
import Globals;
import MapGlobal;
import Map;
import Hierarchy;
import SLK;
import ModificationTables;
import ObjectDataText;
import TableModel;
import TriggerStrings;
import Utilities;
import UnorderedMap;

namespace fs = std::filesystem;

export inline constexpr int hivewe_object_data_format_version = 1;
export inline constexpr size_t max_objects_per_category = 10000;

export enum class ObjectDataCategory {
	unit,
	item,
	ability,
	buff,
	upgrade,
	destructible,
	doodad,
};

export enum class ValidationSeverity {
	error,
	warning,
	info,
};

export struct ValidationIssue {
	ValidationSeverity severity = ValidationSeverity::error;
	std::string category;
	std::string object_id;
	std::string field;
	std::string message;
	std::optional<int> line;
};

export struct ImportValidationReport {
	std::vector<ValidationIssue> issues;

	[[nodiscard]] bool has_blocking_errors() const {
		return std::ranges::any_of(issues, [](const ValidationIssue& issue) {
			return issue.severity == ValidationSeverity::error;
		});
	}

	[[nodiscard]] bool has_warnings() const {
		return std::ranges::any_of(issues, [](const ValidationIssue& issue) {
			return issue.severity == ValidationSeverity::warning;
		});
	}

	void add(ValidationSeverity severity, std::string category, std::string message, std::string object_id = {}, std::string field = {}, std::optional<int> line = {}) {
		issues.push_back({ severity, std::move(category), std::move(object_id), std::move(field), std::move(message), line });
	}
};

export struct ImportConflict {
	ObjectDataCategory category;
	std::string id;
	std::string display_name;
	bool is_custom = false;
};

export struct ObjectCategoryDescriptor {
	ObjectDataCategory category;
	std::string key;
	std::string label;
	std::string main_file;
	std::string skin_file;
	std::string text_file;
	slk::SLK* slk;
	slk::SLK* meta_slk;
	TableModel* table;
	bool optional_ints;
};

export struct ObjectDataImportPackage {
	std::string format;
	std::string source_tileset;
	std::string source_locale;
	std::string source_game_version;
	hive::unordered_map<ObjectDataCategory, ModificationShadowMap> categories;
};

export struct ObjectDataApplyPlan {
	hive::unordered_map<ObjectDataCategory, ModificationShadowMap> merged;
	std::vector<ImportConflict> conflicts;
	ImportValidationReport validation;
};

namespace {

struct Sha256Context {
	std::array<uint32_t, 8> state {};
	std::array<uint8_t, 64> buffer {};
	uint64_t total_bits = 0;
	size_t buffer_size = 0;
};

uint32_t rotr(const uint32_t value, const int bits) {
	return (value >> bits) | (value << (32 - bits));
}

void sha256_transform(Sha256Context& context, const std::array<uint8_t, 64>& chunk) {
	static constexpr uint32_t k[64] = {
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
	};

	std::array<uint32_t, 64> words {};
	for (size_t i = 0; i < 16; ++i) {
		words[i] = (chunk[i * 4] << 24) | (chunk[i * 4 + 1] << 16) | (chunk[i * 4 + 2] << 8) | chunk[i * 4 + 3];
	}
	for (size_t i = 16; i < 64; ++i) {
		const uint32_t s0 = rotr(words[i - 15], 7) ^ rotr(words[i - 15], 18) ^ (words[i - 15] >> 3);
		const uint32_t s1 = rotr(words[i - 2], 17) ^ rotr(words[i - 2], 19) ^ (words[i - 2] >> 10);
		words[i] = words[i - 16] + s0 + words[i - 7] + s1;
	}

	uint32_t a = context.state[0];
	uint32_t b = context.state[1];
	uint32_t c = context.state[2];
	uint32_t d = context.state[3];
	uint32_t e = context.state[4];
	uint32_t f = context.state[5];
	uint32_t g = context.state[6];
	uint32_t h = context.state[7];

	for (size_t i = 0; i < 64; ++i) {
		const uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
		const uint32_t ch = (e & f) ^ ((~e) & g);
		const uint32_t temp1 = h + s1 + ch + k[i] + words[i];
		const uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
		const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
		const uint32_t temp2 = s0 + maj;

		h = g;
		g = f;
		f = e;
		e = d + temp1;
		d = c;
		c = b;
		b = a;
		a = temp1 + temp2;
	}

	context.state[0] += a;
	context.state[1] += b;
	context.state[2] += c;
	context.state[3] += d;
	context.state[4] += e;
	context.state[5] += f;
	context.state[6] += g;
	context.state[7] += h;
}

Sha256Context sha256_init() {
	Sha256Context context;
	context.state = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
	return context;
}

void sha256_update(Sha256Context& context, const std::span<const u8> data) {
	for (const u8 byte : data) {
		context.buffer[context.buffer_size++] = byte;
		context.total_bits += 8;
		if (context.buffer_size == 64) {
			sha256_transform(context, context.buffer);
			context.buffer_size = 0;
		}
	}
}

std::string sha256_final(Sha256Context context) {
	context.buffer[context.buffer_size++] = 0x80;
	if (context.buffer_size > 56) {
		while (context.buffer_size < 64) {
			context.buffer[context.buffer_size++] = 0;
		}
		sha256_transform(context, context.buffer);
		context.buffer_size = 0;
	}
	while (context.buffer_size < 56) {
		context.buffer[context.buffer_size++] = 0;
	}

	const uint64_t bit_length = context.total_bits;
	for (int i = 7; i >= 0; --i) {
		context.buffer[56 + (7 - i)] = static_cast<uint8_t>((bit_length >> (i * 8)) & 0xff);
	}
	sha256_transform(context, context.buffer);

	std::string digest;
	digest.reserve(64);
	for (const uint32_t word : context.state) {
		digest += std::format("{:08x}", word);
	}
	return digest;
}

std::string sha256_bytes(const std::span<const u8> data) {
	auto context = sha256_init();
	sha256_update(context, data);
	return sha256_final(context);
}

std::string sha256_file(const fs::path& path) {
	const auto file = read_file(path);
	if (!file) {
		return {};
	}
	return sha256_bytes(file->buffer);
}

std::expected<void, std::string> write_bytes_file(const fs::path& path, const std::span<const u8> data) {
	std::error_code ec;
	fs::create_directories(path.parent_path(), ec);

	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream.is_open()) {
		return std::unexpected("Unable to write file: " + path.string());
	}

	stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
	if (!stream) {
		return std::unexpected("Error writing file: " + path.string());
	}
	return {};
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

ObjectCategoryDescriptor make_descriptor(
	ObjectDataCategory category,
	std::string key,
	std::string label,
	std::string main_file,
	std::string skin_file,
	std::string text_file,
	slk::SLK* slk,
	slk::SLK* meta_slk,
	TableModel* table,
	const bool optional_ints
) {
	return { category, std::move(key), std::move(label), std::move(main_file), std::move(skin_file), std::move(text_file), slk, meta_slk, table, optional_ints };
}

std::string field_type_for(const slk::SLK& slk, const slk::SLK& meta_slk, const std::string_view id, const std::string_view field) {
	const auto meta_id = slk.field_to_meta_id(meta_slk, field, id);
	if (!meta_id) {
		return {};
	}
	return std::string(meta_slk.data<std::string_view>("type", *meta_id));
}

bool validate_field_value(const std::string_view type, const std::string_view value) {
	if (type.empty()) {
		return false;
	}

	try {
		if (type == "int" || type == "bool" || type.ends_with("Flags") || type == "attackBits" || type == "channelType" || type == "deathType" || type == "defenseTypeInt" || type == "detectionType" || type == "spellDetail" || type == "teamColor" || type == "techAvail") {
			(void)std::stoi(std::string(value));
			return true;
		}
		if (type == "real" || type == "unreal") {
			(void)std::stof(std::string(value));
			return true;
		}
	} catch (const std::exception&) {
		return false;
	}

	return true;
}

void validate_cross_reference(const ObjectCategoryDescriptor& descriptor, const std::string& field, const std::string_view value, ImportValidationReport& report) {
	const auto warn_missing = [&](const std::string_view id, const char* label) {
		if (id.empty()) {
			return;
		}
		if (id.size() == 4 && !units_slk.row_headers.contains(id) && !items_slk.row_headers.contains(id) && !abilities_slk.row_headers.contains(id)
			&& !buff_slk.row_headers.contains(id) && !upgrade_slk.row_headers.contains(id) && !destructibles_slk.row_headers.contains(id)
			&& !doodads_slk.row_headers.contains(id)) {
			report.add(
				ValidationSeverity::warning,
				descriptor.key,
				std::format("{} references unknown {} '{}'.", field, label, id)
			);
		}
	};

	if (field.contains("abil") || field == "reqabils") {
		for (const auto part : std::views::split(value, ',')) {
			warn_missing(std::string_view(part.begin(), part.end()), "ability");
		}
	} else if (field.contains("unit") || field == "builds" || field == "creates" || field == "summon") {
		for (const auto part : std::views::split(value, ',')) {
			const std::string_view token(part.begin(), part.end());
			if (!token.empty()) {
				warn_missing(token, "unit");
			}
		}
	} else if (field.contains("buff")) {
		for (const auto part : std::views::split(value, ',')) {
			warn_missing(std::string_view(part.begin(), part.end()), "buff");
		}
	} else if (field.contains("upgrade")) {
		for (const auto part : std::views::split(value, ',')) {
			warn_missing(std::string_view(part.begin(), part.end()), "upgrade");
		}
	}

	if (value.starts_with("TRIGSTR") && map && map->trigger_strings.string(value).empty()) {
		report.add(ValidationSeverity::warning, descriptor.key, std::format("String reference '{}' is not present in the current map.", value), {}, field);
	}
}

bool shadow_object_fields_equivalent(
	const ObjectCategoryDescriptor& descriptor,
	const std::string& object_id,
	const hive::unordered_map<std::string, std::string>& imported,
	const hive::unordered_map<std::string, std::string>& current
) {
	if (imported.size() != current.size()) {
		return false;
	}

	const TriggerStrings* trigger_strings = map ? &map->trigger_strings : nullptr;
	for (const auto& [field, imported_value] : imported) {
		const auto found = current.find(field);
		if (found == current.end()) {
			return false;
		}

		const auto meta_id = descriptor.slk->field_to_meta_id(*descriptor.meta_slk, field, object_id);
		const std::string_view type = meta_id ? descriptor.meta_slk->data<std::string_view>("type", *meta_id) : std::string_view {};
		if (!import_value_matches_current(imported_value, found->second, type, trigger_strings)) {
			return false;
		}
	}

	return true;
}

} // namespace

export std::vector<ObjectCategoryDescriptor> object_data_category_descriptors() {
	return {
		make_descriptor(ObjectDataCategory::unit, "unit", "Unit", "war3map.w3u", "war3mapSkin.w3u", "unit.ini", &units_slk, &units_meta_slk, units_table, false),
		make_descriptor(ObjectDataCategory::item, "item", "Item", "war3map.w3t", "war3mapSkin.w3t", "item.ini", &items_slk, &items_meta_slk, items_table, false),
		make_descriptor(ObjectDataCategory::ability, "ability", "Ability", "war3map.w3a", "war3mapSkin.w3a", "ability.ini", &abilities_slk, &abilities_meta_slk, abilities_table, true),
		make_descriptor(ObjectDataCategory::buff, "buff", "Buff", "war3map.w3h", "war3mapSkin.w3h", "buff.ini", &buff_slk, &buff_meta_slk, buff_table, false),
		make_descriptor(ObjectDataCategory::upgrade, "upgrade", "Upgrade", "war3map.w3q", "war3mapSkin.w3q", "upgrade.ini", &upgrade_slk, &upgrade_meta_slk, upgrade_table, true),
		make_descriptor(ObjectDataCategory::destructible, "destructible", "Destructible", "war3map.w3b", "war3mapSkin.w3b", "destructible.ini", &destructibles_slk, &destructibles_meta_slk, destructibles_table, false),
		make_descriptor(ObjectDataCategory::doodad, "doodad", "Doodad", "war3map.w3d", "war3mapSkin.w3d", "doodad.ini", &doodads_slk, &doodads_meta_slk, doodads_table, true),
	};
}

export const ObjectCategoryDescriptor* find_object_category_descriptor(const ObjectDataCategory category) {
	static const std::vector<ObjectCategoryDescriptor> descriptors = object_data_category_descriptors();
	for (const auto& descriptor : descriptors) {
		if (descriptor.category == category) {
			return &descriptor;
		}
	}
	return nullptr;
}

export std::string object_category_display_name(const ObjectCategoryDescriptor& descriptor, const std::string& id) {
	if (descriptor.table && descriptor.slk->row_headers.contains(id)) {
		if (descriptor.slk->column_headers.contains("name")) {
			const QString name = descriptor.table->data(id, "name").toString();
			if (!name.isEmpty()) {
				return name.toStdString();
			}
		}
		if (descriptor.slk->column_headers.contains("editorname")) {
			const QString name = descriptor.table->data(id, "editorname").toString();
			if (!name.isEmpty()) {
				return name.toStdString();
			}
		}
		if (descriptor.slk->column_headers.contains("bufftip")) {
			const QString name = descriptor.table->data(id, "bufftip").toString();
			if (!name.isEmpty()) {
				return name.toStdString();
			}
		}
	}
	return id;
}

export ImportValidationReport validate_shadow_map(
	const ObjectCategoryDescriptor& descriptor,
	const ModificationShadowMap& shadow_map,
	const bool strict_package
) {
	ImportValidationReport report;

	if (shadow_map.size() > max_objects_per_category) {
		report.add(ValidationSeverity::error, descriptor.key, std::format("Too many objects ({}) in category.", shadow_map.size()));
		return report;
	}

	for (const auto& [object_id, fields] : shadow_map) {
		if (!is_valid_object_id(object_id)) {
			report.add(ValidationSeverity::error, descriptor.key, "Object ID must be exactly four alphanumeric characters.", object_id);
			continue;
		}

		const bool is_custom = fields.contains("oldid");
		if (is_custom) {
			const std::string& parent = fields.at("oldid");
			if (!is_valid_object_id(parent)) {
				report.add(ValidationSeverity::error, descriptor.key, "Parent ID must be exactly four alphanumeric characters.", object_id, "oldid");
			} else if (!descriptor.slk->row_headers.contains(parent) && !descriptor.slk->base_data.contains(parent)) {
				report.add(ValidationSeverity::error, descriptor.key, std::format("Parent '{}' was not found in base data.", parent), object_id, "oldid");
			}
		} else if (!descriptor.slk->row_headers.contains(object_id) && !descriptor.slk->base_data.contains(object_id)) {
			report.add(ValidationSeverity::error, descriptor.key, "Modified object was not found in base data.", object_id);
		}

		size_t field_count = 0;
		for (const auto& [field, value] : fields) {
			if (field == "oldid") {
				continue;
			}
			field_count++;
			if (!descriptor.slk->field_to_meta_id(*descriptor.meta_slk, field, object_id)) {
				report.add(ValidationSeverity::error, descriptor.key, std::format("Unknown field '{}'.", field), object_id, field);
				continue;
			}

			const std::string type = field_type_for(*descriptor.slk, *descriptor.meta_slk, object_id, field);
			if (!validate_field_value(type, value)) {
				report.add(ValidationSeverity::error, descriptor.key, std::format("Value '{}' is invalid for type '{}'.", value, type), object_id, field);
			}

			validate_cross_reference(descriptor, field, value, report);
		}

		if (field_count == 0 && !is_custom) {
			report.add(ValidationSeverity::warning, descriptor.key, "Object has no modified fields.", object_id);
		}
	}

	if (strict_package && !report.has_blocking_errors()) {
		const slk::SLK scratch = shadow_map_to_slk(*descriptor.slk, shadow_map);
		for (const auto& error : collect_save_modification_file_errors(scratch, *descriptor.meta_slk, descriptor.optional_ints)) {
			report.add(ValidationSeverity::error, descriptor.key, error.message, error.object_id, error.field);
		}
	}

	return report;
}

export std::vector<ImportConflict> detect_import_conflicts(
	const ObjectCategoryDescriptor& descriptor,
	const ModificationShadowMap& imported
) {
	std::vector<ImportConflict> conflicts;
	for (const auto& [object_id, fields] : imported) {
		const auto current = descriptor.slk->shadow_data.find(object_id);
		if (current == descriptor.slk->shadow_data.end()) {
			continue;
		}
		if (shadow_object_fields_equivalent(descriptor, object_id, fields, current->second)) {
			continue;
		}

		ImportConflict conflict;
		conflict.category = descriptor.category;
		conflict.id = object_id;
		conflict.display_name = object_category_display_name(descriptor, object_id);
		conflict.is_custom = fields.contains("oldid");
		conflicts.push_back(std::move(conflict));
	}
	return conflicts;
}

export ModificationShadowMap merge_import_category(
	const ModificationShadowMap& current,
	const ModificationShadowMap& imported,
	const std::unordered_set<std::string>& overwrite_ids
) {
	ModificationShadowMap merged = current;
	for (const auto& [object_id, fields] : imported) {
		if (merged.contains(object_id) && !overwrite_ids.contains(object_id)) {
			continue;
		}
		merged[object_id] = fields;
	}
	return merged;
}

export ImportValidationReport dry_run_import_plan(const ObjectDataApplyPlan& plan) {
	ImportValidationReport report;

	for (const auto& descriptor : object_data_category_descriptors()) {
		const auto found = plan.merged.find(descriptor.category);
		if (found == plan.merged.end()) {
			continue;
		}

		const slk::SLK scratch = shadow_map_to_slk(*descriptor.slk, found->second);
		for (const auto& error : collect_save_modification_file_errors(scratch, *descriptor.meta_slk, descriptor.optional_ints)) {
			report.add(ValidationSeverity::error, descriptor.key, error.message, error.object_id, error.field);
		}
	}

	return report;
}

export ObjectDataApplyPlan build_import_plan(
	const ObjectDataImportPackage& package,
	const std::unordered_set<std::string>& overwrite_ids
) {
	ObjectDataApplyPlan plan;

	for (const auto& descriptor : object_data_category_descriptors()) {
		const auto imported = package.categories.find(descriptor.category);
		if (imported == package.categories.end()) {
			continue;
		}

		ModificationShadowMap current;
		for (const auto& [object_id, fields] : descriptor.slk->shadow_data) {
			current[object_id] = fields;
		}

		plan.merged[descriptor.category] = merge_import_category(current, imported->second, overwrite_ids);

		for (ImportConflict conflict : detect_import_conflicts(descriptor, imported->second)) {
			plan.conflicts.push_back(std::move(conflict));
		}

		ImportValidationReport category_report = validate_shadow_map(descriptor, imported->second, true);
		plan.validation.issues.insert(plan.validation.issues.end(), category_report.issues.begin(), category_report.issues.end());
	}

	if (map && !package.source_tileset.empty() && !package.source_locale.empty()) {
		const std::string current_version = std::format(
			"{}.{}.{}.{}",
			map->info.game_version_major,
			map->info.game_version_minor,
			map->info.game_version_patch,
			map->info.game_version_build
		);
		if (package.source_tileset != std::string(1, hierarchy.tileset)) {
			plan.validation.add(ValidationSeverity::warning, "package", std::format("Source tileset '{}' differs from open map tileset '{}'.", package.source_tileset, hierarchy.tileset));
		}
		if (package.source_locale != hierarchy.locale) {
			plan.validation.add(ValidationSeverity::warning, "package", std::format("Source locale '{}' differs from open map locale '{}'.", package.source_locale, hierarchy.locale));
		}
		if (!package.source_game_version.empty() && package.source_game_version != current_version) {
			plan.validation.add(ValidationSeverity::warning, "package", std::format("Source game version '{}' differs from open map version '{}'.", package.source_game_version, current_version));
		}
	}

	return plan;
}

export std::expected<ObjectDataImportPackage, ImportValidationReport> load_binary_object_data(
	const fs::path& directory,
	const bool require_manifest
) {
	ObjectDataImportPackage package;
	ImportValidationReport report;

	const fs::path manifest_path = directory / "manifest.json";
	if (require_manifest && !fs::exists(manifest_path)) {
		report.add(ValidationSeverity::error, "package", "Missing manifest.json.");
		return std::unexpected(report);
	}

	if (fs::exists(manifest_path)) {
		const auto manifest_file = read_file(manifest_path);
		if (!manifest_file) {
			report.add(ValidationSeverity::error, "package", manifest_file.error());
			return std::unexpected(report);
		}

		try {
			const nlohmann::json manifest = nlohmann::json::parse(
				std::string(reinterpret_cast<const char*>(manifest_file->buffer.data()), manifest_file->buffer.size())
			);

			package.format = manifest.value("format", "");
			if (manifest.contains("format_version") && manifest["format_version"].get<int>() > hivewe_object_data_format_version) {
				report.add(ValidationSeverity::error, "package", "Manifest format version is newer than this HiveWE build supports.");
			}

			package.source_tileset = manifest.value("source_tileset", "");
			package.source_locale = manifest.value("source_locale", "");
			package.source_game_version = manifest.value("source_game_version", "");

			if (manifest.contains("files")) {
				for (const auto& file_entry : manifest["files"]) {
					const std::string name = file_entry.value("name", "");
					const std::string expected_hash = file_entry.value("sha256", "");
					if (name.empty() || expected_hash.empty()) {
						continue;
					}
					const fs::path file_path = directory / name;
					if (!fs::exists(file_path)) {
						report.add(ValidationSeverity::error, "package", std::format("Manifest references missing file '{}'.", name));
						continue;
					}
					if (sha256_file(file_path) != expected_hash) {
						report.add(ValidationSeverity::error, "package", std::format("Checksum mismatch for '{}'.", name));
					}
				}
			}
		} catch (const std::exception& ex) {
			report.add(ValidationSeverity::error, "package", std::format("Failed to parse manifest.json: {}", ex.what()));
			return std::unexpected(report);
		}
	} else {
		package.format = "hivewe-obj-v1";
	}

	for (const auto& descriptor : object_data_category_descriptors()) {
		ModificationShadowMap combined;
		const fs::path main_path = directory / descriptor.main_file;
		if (fs::exists(main_path)) {
			const auto extracted = extract_modification_shadow_map_path(main_path, *descriptor.slk, *descriptor.meta_slk, descriptor.optional_ints);
			if (!extracted) {
				report.add(ValidationSeverity::error, descriptor.key, extracted.error());
				continue;
			}
			combined = std::move(*extracted);
		}

		const fs::path skin_path = directory / descriptor.skin_file;
		if (fs::exists(skin_path)) {
			const auto extracted = extract_modification_shadow_map_path(skin_path, *descriptor.slk, *descriptor.meta_slk, descriptor.optional_ints);
			if (!extracted) {
				report.add(ValidationSeverity::error, descriptor.key, extracted.error());
				continue;
			}
			for (const auto& [object_id, fields] : *extracted) {
				auto& target = combined[object_id];
				target.insert(fields.begin(), fields.end());
			}
		}

		if (!combined.empty()) {
			package.categories.emplace(descriptor.category, std::move(combined));
			const ImportValidationReport category_report = validate_shadow_map(descriptor, package.categories.at(descriptor.category), true);
			report.issues.insert(report.issues.end(), category_report.issues.begin(), category_report.issues.end());
		}
	}

	if (package.categories.empty()) {
		report.add(ValidationSeverity::error, "package", "No object modification data was found.");
	}

	if (report.has_blocking_errors()) {
		return std::unexpected(report);
	}

	return package;
}

export std::expected<ObjectDataImportPackage, ImportValidationReport> load_text_object_data(const fs::path& directory) {
	ObjectDataImportPackage package;
	ImportValidationReport report;

	const fs::path manifest_path = directory / "manifest.json";
	const fs::path table_directory = directory / "table";
	const bool w3x2lni_format = fs::exists(table_directory) && !fs::exists(manifest_path);

	if (fs::exists(manifest_path)) {
		const auto manifest_file = read_file(manifest_path);
		if (!manifest_file) {
			report.add(ValidationSeverity::error, "package", manifest_file.error());
			return std::unexpected(report);
		}

		try {
			const nlohmann::json manifest = nlohmann::json::parse(
				std::string(reinterpret_cast<const char*>(manifest_file->buffer.data()), manifest_file->buffer.size())
			);
			package.format = manifest.value("format", "hivewe-lni-v1");
			package.source_tileset = manifest.value("source_tileset", "");
			package.source_locale = manifest.value("source_locale", "");
			package.source_game_version = manifest.value("source_game_version", "");
		} catch (const std::exception& ex) {
			report.add(ValidationSeverity::error, "package", std::format("Failed to parse manifest.json: {}", ex.what()));
			return std::unexpected(report);
		}
	} else if (w3x2lni_format) {
		package.format = "w3x2lni-lni";
	} else {
		report.add(ValidationSeverity::error, "package", "Missing manifest.json or table/ directory.");
		return std::unexpected(report);
	}

	const fs::path source_directory = fs::exists(table_directory) ? table_directory : directory;
	const bool use_w3x2lni = package.format == "w3x2lni-lni";

	for (const auto& descriptor : object_data_category_descriptors()) {
		const fs::path ini_path = source_directory / descriptor.text_file;
		if (!fs::exists(ini_path)) {
			continue;
		}

		const ParsedTextTable parsed = parse_object_text_table(ini_path, use_w3x2lni);
		for (const TextParseIssue& issue : parsed.issues) {
			report.add(ValidationSeverity::error, descriptor.key, issue.message, {}, {}, issue.line);
		}

		ModificationShadowMap shadow_map;
		for (const ParsedTextObject& object : parsed.objects) {
			shadow_map[object.id] = object.fields;
		}

		if (!shadow_map.empty()) {
			package.categories.emplace(descriptor.category, std::move(shadow_map));
			const ImportValidationReport category_report = validate_shadow_map(descriptor, package.categories.at(descriptor.category), true);
			report.issues.insert(report.issues.end(), category_report.issues.begin(), category_report.issues.end());
		}
	}

	if (package.categories.empty()) {
		report.add(ValidationSeverity::error, "package", "No object modification data was found.");
	}

	if (report.has_blocking_errors()) {
		return std::unexpected(report);
	}

	return package;
}

export std::expected<void, std::string> export_binary_object_data(const fs::path& directory) {
	if (!map) {
		return std::unexpected("No map is loaded.");
	}

	std::error_code ec;
	fs::create_directories(directory, ec);

	nlohmann::json manifest;
	manifest["format_version"] = hivewe_object_data_format_version;
	manifest["format"] = "hivewe-obj-v1";
	manifest["hivewe_version"] = "HiveWE";
	manifest["exported_at"] = "exported";
	manifest["source_tileset"] = std::string(1, hierarchy.tileset);
	manifest["source_locale"] = hierarchy.locale;
	manifest["source_game_version"] = std::format(
		"{}.{}.{}.{}",
		map->info.game_version_major,
		map->info.game_version_minor,
		map->info.game_version_patch,
		map->info.game_version_build
	);

	nlohmann::json files = nlohmann::json::array();
	nlohmann::json categories = nlohmann::json::array();

	for (const auto& descriptor : object_data_category_descriptors()) {
		if (!modification_table_has_data(*descriptor.slk)) {
			continue;
		}

		categories.push_back(descriptor.key);

		for (const bool skin : { false, true }) {
			const std::string file_name = skin ? descriptor.skin_file : descriptor.main_file;
			const std::vector<u8> buffer = build_modification_file_buffer(*descriptor.slk, *descriptor.meta_slk, descriptor.optional_ints, skin);
			if (const auto result = write_bytes_file(directory / file_name, buffer); !result) {
				return std::unexpected(result.error());
			}

			files.push_back({
				{ "name", file_name },
				{ "sha256", sha256_bytes(buffer) },
				{ "object_count", count_modification_objects(*descriptor.slk) },
			});
		}
	}

	if (categories.empty()) {
		return std::unexpected("No object modification data is present in the current map.");
	}

	manifest["categories"] = categories;
	manifest["files"] = files;

	const std::string manifest_text = manifest.dump(2);
	if (const auto result = write_bytes_file(directory / "manifest.json", std::span<const u8>(reinterpret_cast<const u8*>(manifest_text.data()), manifest_text.size())); !result) {
		return std::unexpected(result.error());
	}

	return {};
}

export std::expected<void, std::string> export_text_object_data(const fs::path& directory) {
	if (!map) {
		return std::unexpected("No map is loaded.");
	}

	std::error_code ec;
	fs::create_directories(directory / "table", ec);

	nlohmann::json manifest;
	manifest["format_version"] = hivewe_object_data_format_version;
	manifest["format"] = "hivewe-lni-v1";
	manifest["hivewe_version"] = "HiveWE";
	manifest["exported_at"] = "exported";
	manifest["source_tileset"] = std::string(1, hierarchy.tileset);
	manifest["source_locale"] = hierarchy.locale;
	manifest["source_game_version"] = std::format(
		"{}.{}.{}.{}",
		map->info.game_version_major,
		map->info.game_version_minor,
		map->info.game_version_patch,
		map->info.game_version_build
	);

	nlohmann::json categories = nlohmann::json::array();

	for (const auto& descriptor : object_data_category_descriptors()) {
		if (!modification_table_has_data(*descriptor.slk)) {
			continue;
		}

		categories.push_back(descriptor.key);
		const std::string contents = build_object_text_table(*descriptor.slk, *descriptor.meta_slk, {
			.w3x2lni_format = false,
			.trigger_strings = &map->trigger_strings,
		});
		if (const auto result = write_text_file(directory / "table" / descriptor.text_file, contents); !result) {
			return std::unexpected(result.error());
		}
	}

	if (categories.empty()) {
		return std::unexpected("No object modification data is present in the current map.");
	}

	manifest["categories"] = categories;
	const std::string manifest_text = manifest.dump(2);
	if (const auto result = write_bytes_file(directory / "manifest.json", std::span<const u8>(reinterpret_cast<const u8*>(manifest_text.data()), manifest_text.size())); !result) {
		return std::unexpected(result.error());
	}

	return {};
}

export bool apply_object_data_plan(const ObjectDataApplyPlan& plan) {
	if (!map) {
		return false;
	}

	struct Snapshot {
		hive::unordered_map<ObjectDataCategory, ModificationShadowMap> shadow;
	};

	Snapshot snapshot;
	for (const auto& descriptor : object_data_category_descriptors()) {
		ModificationShadowMap current;
		for (const auto& [object_id, fields] : descriptor.slk->shadow_data) {
			current[object_id] = fields;
		}
		snapshot.shadow.emplace(descriptor.category, std::move(current));
	}

	auto restore_snapshot = [&]() {
		for (const auto& descriptor : object_data_category_descriptors()) {
			const auto found = snapshot.shadow.find(descriptor.category);
			if (found == snapshot.shadow.end()) {
				continue;
			}

			std::vector<std::string> to_remove;
			for (const auto& [object_id, fields] : descriptor.slk->shadow_data) {
				if (!found->second.contains(object_id)) {
					to_remove.push_back(object_id);
				}
			}

			for (const std::string& object_id : to_remove) {
				if (descriptor.slk->shadow_data.at(object_id).contains("oldid") && descriptor.table) {
					descriptor.table->deleteRow(object_id);
				} else {
					descriptor.slk->shadow_data.erase(object_id);
				}
			}

			for (const auto& [object_id, fields] : found->second) {
				const bool is_custom = fields.contains("oldid");
				if (is_custom && !descriptor.slk->row_headers.contains(object_id)) {
					if (descriptor.table) {
						descriptor.table->copyRow(fields.at("oldid"), object_id);
					} else {
						descriptor.slk->copy_row(fields.at("oldid"), object_id, false);
					}
				}

				for (const auto& [field, value] : fields) {
					descriptor.slk->set_shadow_data(field, object_id, value);
				}

				if (descriptor.table && descriptor.slk->row_headers.contains(object_id)) {
					descriptor.table->refreshRow(object_id);
				}
			}
		}
	};

	try {
		for (const auto& descriptor : object_data_category_descriptors()) {
			const auto found = plan.merged.find(descriptor.category);
			if (found == plan.merged.end()) {
				continue;
			}

			for (const auto& [object_id, fields] : found->second) {
				const bool is_custom = fields.contains("oldid");
				const bool existed = descriptor.slk->shadow_data.contains(object_id);

				if (is_custom) {
					if (existed && descriptor.slk->shadow_data.at(object_id).contains("oldid")
						&& descriptor.slk->shadow_data.at(object_id).at("oldid") != fields.at("oldid") && descriptor.table) {
						descriptor.table->deleteRow(object_id);
					}

					if (!descriptor.slk->row_headers.contains(object_id)) {
						if (descriptor.table) {
							descriptor.table->copyRow(fields.at("oldid"), object_id);
						} else {
							descriptor.slk->copy_row(fields.at("oldid"), object_id, false);
						}
					}
				}

				for (const auto& [field, value] : fields) {
					descriptor.slk->set_shadow_data(field, object_id, value);
					if (descriptor.category == ObjectDataCategory::unit) {
						map->units.process_unit_field_change(object_id, field);
					} else if (descriptor.category == ObjectDataCategory::item) {
						map->units.process_item_field_change(object_id, field);
					}
				}

				if (descriptor.table && descriptor.slk->row_headers.contains(object_id)) {
					descriptor.table->refreshRow(object_id);
				}
			}
		}
	} catch (const std::exception&) {
		restore_snapshot();
		return false;
	}

	return true;
}
