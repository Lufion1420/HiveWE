export module ModificationTables;

import std;
import std.compat;
import BinaryReader;
import BinaryWriter;
import Hierarchy;
import SLK;
import Utilities;
import UnorderedMap;
import <cassert>;

namespace fs = std::filesystem;

constexpr int mod_table_write_version = 3;

export struct ModificationSerializeError {
	std::string object_id;
	std::string field;
	std::string message;
};

/// The type byte the writer uses for a field of this meta data type (0 int, 1 real, 2 unreal, 3 string).
/// equipmentType, itemTag and occlusionType (3.0 additions) fall to 3: their UnitEditorData values are words ("Head", "Shop", "Auto")
/// like weaponType's. Which byte the World Editor writes for them is not verified, so a value read from a file keeps the byte it was
/// read with (see is_unverified_write_type); this default applies only to values edited in HiveWE.
export uint32_t modification_write_type(const std::string_view type) {
	if (type == "int" || type == "bool" || type.ends_with("Flags") || type == "attackBits" || type == "channelType" || type == "deathType"
		|| type == "defenseTypeInt" || type == "detectionType" || type == "spellDetail" || type == "teamColor" || type == "techAvail") {
		return 0;
	} else if (type == "real") {
		return 1;
	} else if (type == "unreal") {
		return 2;
	}
	return 3;
}

/// Meta data types whose type byte in World Editor files is not known. A field of such a type read with another byte than
/// modification_write_type gives is kept raw next to its shadow value and written back with its own bytes while the value is unchanged.
export bool is_unverified_write_type(const std::string_view type) {
	return type == "equipmentType" || type == "itemTag" || type == "occlusionType";
}

namespace {

/// The layout of one modification table, with what is done with it left to the callbacks. `record` holds the field's bytes from its
/// id through its end token, exactly as read.
void walk_modification_table(
	BinaryReader& reader,
	const uint32_t version,
	const bool optional_ints,
	const std::function<void(const std::string& original_id, const std::string& modified_id)>& on_object,
	const std::function<void(
		const std::string& original_id,
		const std::string& modified_id,
		const std::string& modification_id,
		uint32_t type,
		uint32_t level_variation,
		uint32_t data_pointer,
		std::string data,
		std::span<const uint8_t> record
	)>& on_field
) {
	const uint32_t objects = reader.read<uint32_t>();
	for (size_t i = 0; i < objects; i++) {
		const std::string original_id = reader.read_string(4);
		const std::string modified_id = reader.read_string(4);

		if (version >= 3) {
			const uint32_t set_count = reader.read<uint32_t>();
			if (set_count > 1) {
				std::println("Set count of {} detected", set_count);
			}
			const uint32_t set_flag = reader.read<uint32_t>();
		}
		on_object(original_id, modified_id);

		const uint32_t modifications = reader.read<uint32_t>();

		for (size_t j = 0; j < modifications; j++) {
			const size_t record_start = reader.position;
			const std::string modification_id = reader.read_string(4);
			const uint32_t type = reader.read<uint32_t>();

			uint32_t level_variation = 0;
			uint32_t data_pointer = 0;
			if (optional_ints) {
				level_variation = reader.read<uint32_t>();
				data_pointer = reader.read<uint32_t>();
			}

			std::string data;
			switch (type) {
				case 0:
					data = std::to_string(reader.read<int>());
					break;
				case 1:
				case 2:
					data = std::to_string(reader.read<float>());
					break;
				case 3:
					data = reader.read_c_string();
					break;
				default:
					std::println("Unknown data type {} while loading modification table.", type);
			}
			reader.advance(4);

			const std::span<const uint8_t> record(reader.buffer.data() + record_start, reader.position - record_start);
			on_field(original_id, modified_id, modification_id, type, level_variation, data_pointer, std::move(data), record);
		}
	}
}

/// How the readers file one field record.
struct FieldPlacement {
	std::string column; // the shadow column, empty when the meta data does not know the id
	bool keep_raw = false;
};

/// "Unknown" is decided by the meta row before any level or data pointer suffix is added, so an unknown id in a levelled table no longer
/// becomes a column such as "a1".
FieldPlacement place_field(
	const slk::SLK& meta_slk,
	const std::string& modification_id,
	const uint32_t type,
	const uint32_t level_variation,
	const uint32_t data_pointer,
	const bool optional_ints
) {
	if (!meta_slk.row_headers.contains(modification_id)) {
		return { "", true };
	}

	std::string column_header = to_lowercase_copy(meta_slk.data<std::string_view>("field", modification_id));
	if (column_header.empty()) {
		return { "", true };
	}
	if (optional_ints) {
		if (data_pointer != 0) {
			column_header += static_cast<char>('a' + data_pointer - 1);
		}
		if (meta_slk.data<std::string_view>("repeat", modification_id) != "0") {
			column_header += std::to_string(level_variation);
		}
	}

	const std::string_view meta_type = meta_slk.data<std::string_view>("type", modification_id);
	const bool keep_raw = is_unverified_write_type(meta_type) && type != modification_write_type(meta_type);
	return { std::move(column_header), keep_raw };
}

slk::RawModification make_raw(
	const std::string& modification_id,
	const uint32_t type,
	const uint32_t level,
	const uint32_t pointer,
	std::string value,
	std::string column,
	const bool skin,
	const std::span<const uint8_t> record
) {
	slk::RawModification raw;
	raw.field_id = modification_id;
	raw.type = type;
	raw.level = level;
	raw.pointer = pointer;
	raw.value = std::move(value);
	raw.column = std::move(column);
	raw.skin = skin;
	raw.bytes.assign(record.begin(), record.end());
	return raw;
}

} // namespace

/// A custom object that load_modification_table left out because its base object is not in the loaded object data.
export struct SkippedObject {
	std::string file; // empty until load_modification_file fills it in
	std::string id;
	std::string base_id;
};

/// A custom object whose base id is not in the loaded object data (stock campaign maps carry such leftovers, e.g. humanx03's n003 on
/// nbsh) is skipped with all its field changes instead of failing the load; it is not kept, so a save drops it. Each skipped object is
/// appended once to `skipped` when that is given (the file name is left empty here).
/// A field whose id the meta data does not know is kept as its bytes in `slk.raw_modifications` (marked `skin_file`), so a save writes
/// it back unchanged.
export void load_modification_table(
	BinaryReader& reader,
	const uint32_t version,
	slk::SLK& slk,
	const slk::SLK& meta_slk,
	const bool modification,
	const bool optional_ints,
	std::vector<SkippedObject>* skipped = nullptr,
	const bool skin_file = false
) {
	std::set<std::string> skipped_ids;
	walk_modification_table(
		reader,
		version,
		optional_ints,
		[&](const std::string& original_id, const std::string& modified_id) {
			if (modification && !slk.base_data.contains(modified_id)) {
				// copy_row would fail inside at() with no ids in the message.
				if (!slk.base_data.contains(original_id)) {
					skipped_ids.insert(modified_id);
					// Entries of earlier files already carry their file name, so only this call's own (unnamed) ones match.
					if (skipped
						&& std::ranges::none_of(*skipped, [&](const SkippedObject& entry) { return entry.file.empty() && entry.id == modified_id; })) {
						skipped->push_back({ "", modified_id, original_id });
					}
					return;
				}
				slk.copy_row(original_id, modified_id, false);
			}
		},
		[&](const std::string& original_id,
			const std::string& modified_id,
			const std::string& modification_id,
			const uint32_t type,
			const uint32_t level_variation,
			const uint32_t data_pointer,
			std::string data,
			const std::span<const uint8_t> record) {
			if (modification && skipped_ids.contains(modified_id)) {
				return;
			}
			const std::string& object_id = modification ? modified_id : original_id;
			FieldPlacement placement = place_field(meta_slk, modification_id, type, level_variation, data_pointer, optional_ints);
			if (placement.keep_raw) {
				slk.raw_modifications[object_id].push_back(make_raw(modification_id, type, level_variation, data_pointer, data, placement.column, skin_file, record));
			}
			if (!placement.column.empty()) {
				slk.set_shadow_data(placement.column, object_id, std::move(data));
			}
		}
	);
}

/// Walks a whole object data file the way load_modification_file does, without an SLK to apply it to.
export void check_modification_file_layout(BinaryReader& reader, const bool optional_ints) {
	const uint32_t version = reader.read<uint32_t>();
	for (int table = 0; table < 2; table++) {
		walk_modification_table(
			reader,
			version,
			optional_ints,
			[](const std::string&, const std::string&) {},
			[](const std::string&, const std::string&, const std::string&, uint32_t, uint32_t, uint32_t, std::string, std::span<const uint8_t>) {}
		);
	}
}

export std::expected<uint32_t, std::string> peek_modification_file_version(BinaryReader& reader) {
	if (reader.buffer.size() < sizeof(uint32_t)) {
		return std::unexpected("Modification file is too small to contain a version header.");
	}

	const size_t position = reader.position;
	const uint32_t version = reader.read<uint32_t>();
	reader.position = position;

	if (version != 1 && version != 2 && version != 3) {
		return std::unexpected(std::format("Unknown modification table version {}.", version));
	}

	return version;
}

export std::expected<void, std::string> load_modification_file_reader(
	BinaryReader& reader,
	slk::SLK& base_data,
	const slk::SLK& meta_slk,
	const bool optional_ints,
	std::vector<SkippedObject>* skipped = nullptr,
	const bool skin_file = false
) {
	const auto version = peek_modification_file_version(reader);
	if (!version) {
		return std::unexpected(version.error());
	}

	[[maybe_unused]] const uint32_t consumed_version = reader.read<uint32_t>();
	load_modification_table(reader, *version, base_data, meta_slk, false, optional_ints, skipped, skin_file);
	load_modification_table(reader, *version, base_data, meta_slk, true, optional_ints, skipped, skin_file);
	return {};
}

export std::expected<void, std::string> load_modification_file_path(const fs::path& path, slk::SLK& base_data, const slk::SLK& meta_slk, const bool optional_ints) {
	auto file = read_file(path);
	if (!file) {
		return std::unexpected(file.error());
	}

	return load_modification_file_reader(*file, base_data, meta_slk, optional_ints);
}

export void load_modification_file(
	const std::string_view file_name,
	slk::SLK& base_data,
	const slk::SLK& meta_slk,
	const bool optional_ints,
	std::vector<SkippedObject>* skipped = nullptr
) {
	BinaryReader reader = hierarchy.map_file_read(file_name).value();
	const size_t skipped_before = skipped ? skipped->size() : 0;

	const int version = reader.read<uint32_t>();
	if (version != 1 && version != 2 && version != 3) {
		std::cout << "Unknown modification table version of " << version << " detected. Attempting to load, but may crash.\n";
	}

	// Unknown fields of a war3mapSkin.* file are written back to that file.
	const bool skin_file = file_name.starts_with("war3mapSkin.");

	// Map::load's caller shows this text, so it names the file.
	try {
		load_modification_table(reader, version, base_data, meta_slk, false, optional_ints, skipped, skin_file);
		load_modification_table(reader, version, base_data, meta_slk, true, optional_ints, skipped, skin_file);
	} catch (const std::exception& error) {
		throw std::runtime_error(std::format("{}: {}", file_name, error.what()));
	}

	if (skipped) {
		for (size_t i = skipped_before; i < skipped->size(); i++) {
			(*skipped)[i].file = std::string(file_name);
		}
	}
}

namespace {

bool write_modification_property(
	BinaryWriter& sub_writer,
	const slk::SLK& slk,
	const slk::SLK& meta_slk,
	const std::string& id,
	const std::string& property_id,
	const std::string& value,
	const bool optional_ints,
	std::vector<ModificationSerializeError>* errors
) {
	if (property_id == "oldid") {
		return true;
	}

	int variation = 0;
	int data_pointer = 0;

	const auto meta_id2 = slk.field_to_meta_id(meta_slk, property_id, id);
	if (!meta_id2) {
		if (errors) {
			errors->push_back({ id, property_id, "Meta data key not found for property." });
			return false;
		}

		// Used to be exit(0), which closed the editor mid-save with no message. Map::save() checks this before writing anything.
		throw std::runtime_error(std::format("Meta data key not found for id {} property {}", id, property_id));
	}
	const std::string meta_data_key = std::string(meta_id2.value());

	if (optional_ints) {
		const bool has_repeat = meta_slk.data<std::string_view>("repeat", meta_data_key) != "0";
		const bool append_index =
			meta_slk.column_headers.contains("appendindex") && meta_slk.data<int>("appendindex", meta_data_key) > 0;

		if (has_repeat && !append_index) {
			size_t nr_position = property_id.find_first_of("0123456789");
			if (nr_position != std::string::npos) {
				const std::string_view digits = std::string_view(property_id).substr(nr_position);
				const auto [end, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), variation);
				if (ec != std::errc()) {
					const std::string message = std::format("Level number '{}' is not valid.", digits);
					if (errors) {
						errors->push_back({ id, property_id, message });
						return false;
					}
					throw std::runtime_error(std::format("{} Object {} property {}", message, id, property_id));
				}
			}
		}

		if (property_id.starts_with("data") && property_id.size() > 4 && std::isalpha(static_cast<unsigned char>(property_id[4]))) {
			data_pointer = property_id[4] - 'a' + 1;
		}
	}

	sub_writer.write_string(meta_data_key);
	for (size_t i = 0; i < 4 - meta_data_key.size(); i++) {
		sub_writer.write<uint8_t>('\0');
	}

	const std::string_view type = meta_slk.data<std::string_view>("type", meta_data_key);
	const uint32_t write_type = modification_write_type(type);

	sub_writer.write<uint32_t>(write_type);

	if (optional_ints) {
		sub_writer.write<uint32_t>(variation);
		sub_writer.write<uint32_t>(data_pointer);
	}

	try {
		if (write_type == 0) {
			sub_writer.write<int>(std::stoi(value));
		} else if (write_type == 1 || write_type == 2) {
			sub_writer.write<float>(std::stof(value));
		} else {
			sub_writer.write_c_string(value);
		}
	} catch (const std::exception&) {
		if (errors) {
			errors->push_back({ id, property_id, std::format("Value '{}' is not valid for type '{}'.", value, type) });
			return false;
		}
		throw;
	}

	sub_writer.write<uint32_t>(0);
	return true;
}


/// Whether `raw` belongs in this file: an unknown field (no column) goes back to the file it was read from.
bool raw_goes_to_file(const slk::RawModification& raw, const bool skin) {
	return raw.column.empty() && raw.skin == skin;
}

/// The one table writer. Without `errors` a field that cannot be written throws; with it the field is left out and reported.
/// Objects come in shadow_data order, then objects that hold only raw records for this file. A skin file gets no shadow fields, only the
/// unknown fields read from a skin file; a main file gets the shadow fields (a raw record stands in for its column while the value is the
/// one it was read with) and then the unknown fields read from a main file.
void write_modification_table(
	BinaryWriter& writer,
	const slk::SLK& slk,
	const slk::SLK& meta_slk,
	const bool custom,
	const bool optional_ints,
	const bool skin,
	std::vector<ModificationSerializeError>* errors
) {
	static const std::vector<slk::RawModification> no_raw;
	const auto raw_of = [&](const std::string& id) -> const std::vector<slk::RawModification>& {
		const auto found = slk.raw_modifications.find(id);
		return found == slk.raw_modifications.end() ? no_raw : found->second;
	};

	BinaryWriter sub_writer;
	size_t count = 0;

	const auto write_object = [&](const std::string& id, const hive::unordered_map<std::string, std::string>* properties) {
		count++;
		if (custom) {
			sub_writer.write_string(properties->at("oldid"));
			sub_writer.write_string(id);
		} else {
			sub_writer.write_string(id);
			sub_writer.write<uint32_t>(0);
		}

		sub_writer.write<uint32_t>(1);
		sub_writer.write<uint32_t>(0);

		const std::vector<slk::RawModification>& raw = raw_of(id);
		size_t written_modifications = 0;
		BinaryWriter property_writer;

		if (!skin && properties) {
			for (const auto& [property_id, value] : *properties) {
				if (property_id == "oldid") {
					continue;
				}
				const auto standing_in = std::ranges::find_if(raw, [&](const slk::RawModification& record) {
					return record.column == property_id && record.value == value;
				});
				if (standing_in != raw.end()) {
					property_writer.buffer.insert(property_writer.buffer.end(), standing_in->bytes.begin(), standing_in->bytes.end());
					written_modifications++;
					continue;
				}

				const size_t before = property_writer.buffer.size();
				if (!write_modification_property(property_writer, slk, meta_slk, id, property_id, value, optional_ints, errors)) {
					property_writer.buffer.resize(before);
					continue;
				}
				written_modifications++;
			}
		}

		for (const slk::RawModification& record : raw) {
			if (raw_goes_to_file(record, skin)) {
				property_writer.buffer.insert(property_writer.buffer.end(), record.bytes.begin(), record.bytes.end());
				written_modifications++;
			}
		}

		sub_writer.write<uint32_t>(written_modifications);
		sub_writer.buffer.insert(sub_writer.buffer.end(), property_writer.buffer.begin(), property_writer.buffer.end());
	};

	for (const auto& [id, properties] : slk.shadow_data) {
		if (custom != properties.contains("oldid")) {
			continue;
		}
		write_object(id, &properties);
	}

	// An original object whose only changes are raw records has no shadow_data entry. A custom object always has one (its oldid).
	if (!custom) {
		for (const auto& [id, records] : slk.raw_modifications) {
			if (slk.shadow_data.contains(id)) {
				continue;
			}
			if (std::ranges::any_of(records, [&](const slk::RawModification& record) { return raw_goes_to_file(record, skin); })) {
				write_object(id, nullptr);
			}
		}
	}

	writer.write<uint32_t>(count);
	writer.write_vector(sub_writer.buffer);
}

} // namespace

export void save_modification_table(BinaryWriter& writer, const slk::SLK& slk, const slk::SLK& meta_slk, const bool custom, const bool optional_ints, const bool skin) {
	write_modification_table(writer, slk, meta_slk, custom, optional_ints, skin, nullptr);
}

export std::vector<ModificationSerializeError> save_modification_table_collect_errors(
	BinaryWriter& writer,
	const slk::SLK& slk,
	const slk::SLK& meta_slk,
	const bool custom,
	const bool optional_ints,
	const bool skin
) {
	std::vector<ModificationSerializeError> errors;
	write_modification_table(writer, slk, meta_slk, custom, optional_ints, skin, &errors);
	return errors;
}

export std::vector<u8> build_modification_file_buffer(const slk::SLK& slk, const slk::SLK& meta_slk, const bool optional_ints, const bool skin) {
	BinaryWriter writer;
	writer.write<uint32_t>(mod_table_write_version);

	save_modification_table(writer, slk, meta_slk, false, optional_ints, skin);
	save_modification_table(writer, slk, meta_slk, true, optional_ints, skin);

	return std::vector<u8>(writer.buffer.begin(), writer.buffer.end());
}

export std::vector<ModificationSerializeError> collect_save_modification_file_errors(const slk::SLK& slk, const slk::SLK& meta_slk, const bool optional_ints) {
	std::vector<ModificationSerializeError> errors;
	BinaryWriter writer;
	writer.write<uint32_t>(mod_table_write_version);

	const auto append = [&](const bool custom, const bool skin) {
		BinaryWriter section;
		auto batch = save_modification_table_collect_errors(section, slk, meta_slk, custom, optional_ints, skin);
		errors.insert(errors.end(), batch.begin(), batch.end());
	};

	append(false, false);
	append(true, false);
	append(false, true);
	append(true, true);
	return errors;
}

export void save_modification_file(const std::string_view file_name, const slk::SLK& slk, const slk::SLK& meta_slk, const bool optional_ints, const bool skin) {
	hierarchy.map_file_write(file_name, build_modification_file_buffer(slk, meta_slk, optional_ints, skin));
}

export size_t count_modification_objects(const slk::SLK& slk) {
	size_t count = slk.shadow_data.size();
	for (const auto& [id, records] : slk.raw_modifications) {
		if (!slk.shadow_data.contains(id) && !records.empty()) {
			count++;
		}
	}
	return count;
}

export bool modification_table_has_data(const slk::SLK& slk) {
	return !slk.shadow_data.empty() || std::ranges::any_of(slk.raw_modifications, [](const auto& entry) { return !entry.second.empty(); });
}

export using ModificationShadowMap = hive::unordered_map<std::string, hive::unordered_map<std::string, std::string>>;

/// A custom object whose base id is not in the template tables, kept as its raw bytes (normalized to the version 3 layout that
/// build_modification_file_buffer writes) so a rewrite of the file can carry it over unchanged.
export struct RawCustomObject {
	std::string id;
	std::string base_id;
	std::vector<u8> bytes;
};

/// With `preserved` given, a custom object whose base id is unknown is not an error: it is left out of the returned map and its raw
/// bytes are appended to `preserved` (its field changes are not interpreted at all).
/// With `raw_fields` given, the fields load_modification_table keeps raw are appended there (skin=false) and their object gets an entry
/// in the returned map even when it has no other field; without it they are left out.
export std::expected<ModificationShadowMap, std::string> extract_modification_shadow_map(
	BinaryReader reader,
	const slk::SLK& template_slk,
	const slk::SLK& meta_slk,
	const bool optional_ints,
	std::vector<RawCustomObject>* preserved = nullptr,
	slk::RawModificationMap* raw_fields = nullptr
) {
	const auto version = peek_modification_file_version(reader);
	if (!version) {
		return std::unexpected(version.error());
	}

	ModificationShadowMap shadow_map;

	auto extract_table = [&](const bool modification) {
		const uint32_t objects = reader.read<uint32_t>();
		if (objects > 10000) {
			throw std::runtime_error(std::format("Too many objects ({}) in modification table.", objects));
		}

		for (size_t i = 0; i < objects; i++) {
			const size_t object_start = reader.position;
			const std::string original_id = reader.read_string(4);
			const std::string modified_id = reader.read_string(4);

			size_t body_start = reader.position;
			if (*version >= 3) {
				[[maybe_unused]] const uint32_t set_count = reader.read<uint32_t>();
				[[maybe_unused]] const uint32_t set_flags = reader.read<uint32_t>();
				body_start = reader.position - 2 * sizeof(uint32_t);
			}

			const std::string& object_id = modification ? modified_id : original_id;
			const bool preserve_raw = modification && preserved && !template_slk.row_headers.contains(original_id)
				&& !template_slk.base_data.contains(original_id);
			if (modification && !preserve_raw) {
				shadow_map[object_id]["oldid"] = original_id;
			}

			const uint32_t modifications = reader.read<uint32_t>();
			if (modifications > 10000) {
				throw std::runtime_error(std::format("Too many modifications ({}) for object {}.", modifications, object_id));
			}

			for (size_t j = 0; j < modifications; j++) {
				const size_t record_start = reader.position;
				const std::string modification_id = reader.read_string(4);
				const uint32_t type = reader.read<uint32_t>();

				uint32_t level_variation = 0;
				uint32_t data_pointer = 0;
				if (optional_ints) {
					level_variation = reader.read<uint32_t>();
					data_pointer = reader.read<uint32_t>();
				}

				std::string data;
				switch (type) {
					case 0:
						data = std::to_string(reader.read<int>());
						break;
					case 1:
					case 2:
						data = std::to_string(reader.read<float>());
						break;
					case 3:
						data = reader.read_c_string();
						break;
					default:
						throw std::runtime_error(std::format("Unknown data type {} while loading modification table.", type));
				}
				reader.advance(4);

				if (preserve_raw) {
					continue;
				}

				FieldPlacement placement = place_field(meta_slk, modification_id, type, level_variation, data_pointer, optional_ints);
				if (placement.keep_raw && raw_fields) {
					const std::span<const uint8_t> record(reader.buffer.data() + record_start, reader.position - record_start);
					(*raw_fields)[object_id].push_back(make_raw(modification_id, type, level_variation, data_pointer, data, placement.column, false, record));
					(void)shadow_map[object_id];
				}
				if (!placement.column.empty()) {
					shadow_map[object_id][placement.column] = std::move(data);
				}
			}

			if (preserve_raw) {
				RawCustomObject raw{ modified_id, original_id, {} };
				raw.bytes.assign(reader.buffer.begin() + object_start, reader.buffer.begin() + object_start + 2 * 4);
				if (*version < 3) {
					for (const uint32_t value : { 1u, 0u }) {
						const auto* p = reinterpret_cast<const u8*>(&value);
						raw.bytes.insert(raw.bytes.end(), p, p + sizeof(value));
					}
				}
				raw.bytes.insert(raw.bytes.end(), reader.buffer.begin() + body_start,reader.buffer.begin() + reader.position);
				preserved->push_back(std::move(raw));
			}
		}
	};

	try {
		[[maybe_unused]] const uint32_t consumed_version = reader.read<uint32_t>();
		extract_table(false);
		extract_table(true);
	} catch (const std::exception& ex) {
		return std::unexpected(ex.what());
	}

	for (const auto& [object_id, fields] : shadow_map) {
		if (fields.contains("oldid")) {
			const std::string& parent = fields.at("oldid");
			if (!template_slk.row_headers.contains(parent) && !template_slk.base_data.contains(parent)) {
				return std::unexpected(std::format("Custom object '{}' references unknown parent '{}'.", object_id, parent));
			}
		} else if (!template_slk.row_headers.contains(object_id) && !template_slk.base_data.contains(object_id)) {
			return std::unexpected(std::format("Modified object '{}' does not exist in base data.", object_id));
		}
	}

	return shadow_map;
}

export std::expected<ModificationShadowMap, std::string> extract_modification_shadow_map_path(
	const fs::path& path,
	const slk::SLK& template_slk,
	const slk::SLK& meta_slk,
	const bool optional_ints,
	std::vector<RawCustomObject>* preserved = nullptr,
	slk::RawModificationMap* raw_fields = nullptr
) {
	const auto file = read_file(path);
	if (!file) {
		return std::unexpected(file.error());
	}
	return extract_modification_shadow_map(std::move(file.value()), template_slk, meta_slk, optional_ints, preserved, raw_fields);
}

/// Like build_modification_file_buffer, with the raw custom objects appended to the custom table unchanged.
export std::vector<u8> build_modification_file_buffer_with_raw(
	const slk::SLK& slk,
	const slk::SLK& meta_slk,
	const bool optional_ints,
	const bool skin,
	const std::vector<RawCustomObject>& raw_objects
) {
	BinaryWriter writer;
	writer.write<uint32_t>(mod_table_write_version);
	save_modification_table(writer, slk, meta_slk, false, optional_ints, skin);

	BinaryWriter custom;
	save_modification_table(custom, slk, meta_slk, true, optional_ints, skin);
	uint32_t count = 0;
	std::memcpy(&count, custom.buffer.data(), sizeof(count));
	count += static_cast<uint32_t>(raw_objects.size());
	std::memcpy(custom.buffer.data(), &count, sizeof(count));
	for (const RawCustomObject& raw : raw_objects) {
		custom.buffer.insert(custom.buffer.end(), raw.bytes.begin(), raw.bytes.end());
	}
	writer.buffer.insert(writer.buffer.end(), custom.buffer.begin(), custom.buffer.end());

	return std::vector<u8>(writer.buffer.begin(), writer.buffer.end());
}

/// `raw_fields` (from extract_modification_shadow_map) become the scratch table's raw records, so its writers keep them.
export slk::SLK shadow_map_to_slk(
	const slk::SLK& template_slk,
	const ModificationShadowMap& shadow_map,
	const slk::RawModificationMap* raw_fields = nullptr
) {
	slk::SLK scratch;
	if (raw_fields) {
		scratch.raw_modifications = *raw_fields;
	}
	scratch.base_data = template_slk.base_data;
	scratch.row_headers = template_slk.row_headers;
	scratch.column_headers = template_slk.column_headers;
	scratch.index_to_row = template_slk.index_to_row;
	scratch.index_to_column = template_slk.index_to_column;

	for (const auto& [object_id, fields] : shadow_map) {
		if (fields.contains("oldid") && !scratch.row_headers.contains(object_id)) {
			const std::string& parent = fields.at("oldid");
			// copy_row() unconditionally does base_data.at(row_header), which throws std::out_of_range
			// if the row isn't in base_data specifically - a row can be present in row_headers without
			// a base_data entry (e.g. a header declared with no cell data), so the guard here must
			// match that exact precondition, not treat "in row_headers" as an equally-safe substitute.
			if (scratch.base_data.contains(parent)) {
				scratch.copy_row(parent, object_id, false);
			}
		}
		for (const auto& [field, value] : fields) {
			scratch.set_shadow_data(field, object_id, value);
		}
	}

	return scratch;
}

/// Whether two raw records would be written as the same field of the same file: same id, level and data pointer, and both going to the
/// main file (a record standing in for a known column always does) or both to the skin file.
export bool same_raw_field(const slk::RawModification& a, const slk::RawModification& b) {
	const bool a_skin = a.column.empty() && a.skin;
	const bool b_skin = b.column.empty() && b.skin;
	return a.field_id == b.field_id && a.level == b.level && a.pointer == b.pointer && a_skin == b_skin;
}

/// Adds imported raw records to an object's own. With `overwrite` (the import replaces an object the map already has) the object's
/// unknown-field records are dropped first; records standing in for a known column stay unless the import brings the same field. A
/// record of the same field (same_raw_field) is always replaced, so no field is written twice.
export void merge_imported_raw_records(
	std::vector<slk::RawModification>& target,
	const std::vector<slk::RawModification>& imported,
	const bool overwrite
) {
	if (overwrite) {
		std::erase_if(target, [](const slk::RawModification& record) { return record.column.empty(); });
	}
	for (const slk::RawModification& record : imported) {
		std::erase_if(target, [&](const slk::RawModification& existing) { return same_raw_field(existing, record); });
		target.push_back(record);
	}
}

/// Whether two objects hold the same unknown-field records, in any order. Records standing in for a known column are left out: their
/// shadow value is compared instead.
export bool same_unknown_raw_records(const std::vector<slk::RawModification>& a, const std::vector<slk::RawModification>& b) {
	const auto unknown = [](const std::vector<slk::RawModification>& records) {
		std::vector<std::pair<bool, std::vector<uint8_t>>> result;
		for (const slk::RawModification& record : records) {
			if (record.column.empty()) {
				result.emplace_back(record.skin, record.bytes);
			}
		}
		std::ranges::sort(result);
		return result;
	};
	return unknown(a) == unknown(b);
}
