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

export void load_modification_table(BinaryReader& reader, const uint32_t version, slk::SLK& slk, const slk::SLK& meta_slk, const bool modification, const bool optional_ints) {
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
		if (modification && !slk.base_data.contains(modified_id)) {
			slk.copy_row(original_id, modified_id, false);
		}

		const uint32_t modifications = reader.read<uint32_t>();

		for (size_t j = 0; j < modifications; j++) {
			const std::string modification_id = reader.read_string(4);
			const uint32_t type = reader.read<uint32_t>();

			std::string column_header = to_lowercase_copy(meta_slk.data<std::string_view>("field", modification_id));
			if (optional_ints) {
				const uint32_t level_variation = reader.read<uint32_t>();
				const uint32_t data_pointer = reader.read<uint32_t>();
				if (data_pointer != 0) {
					column_header += static_cast<char>('a' + data_pointer - 1);
				}
				if (meta_slk.data<std::string_view>("repeat", modification_id) != "0") {
					column_header += std::to_string(level_variation);
				}
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

			if (column_header == "") {
				std::println("Unknown mod id: {}", modification_id);
				continue;
			}

			if (modification) {
				slk.set_shadow_data(column_header, modified_id, data);
			} else {
				slk.set_shadow_data(column_header, original_id, data);
			}
		}
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

export std::expected<void, std::string> load_modification_file_reader(BinaryReader& reader, slk::SLK& base_data, const slk::SLK& meta_slk, const bool optional_ints) {
	const auto version = peek_modification_file_version(reader);
	if (!version) {
		return std::unexpected(version.error());
	}

	[[maybe_unused]] const uint32_t consumed_version = reader.read<uint32_t>();
	load_modification_table(reader, *version, base_data, meta_slk, false, optional_ints);
	load_modification_table(reader, *version, base_data, meta_slk, true, optional_ints);
	return {};
}

export std::expected<void, std::string> load_modification_file_path(const fs::path& path, slk::SLK& base_data, const slk::SLK& meta_slk, const bool optional_ints) {
	auto file = read_file(path);
	if (!file) {
		return std::unexpected(file.error());
	}

	return load_modification_file_reader(*file, base_data, meta_slk, optional_ints);
}

export void load_modification_file(const std::string_view file_name, slk::SLK& base_data, const slk::SLK& meta_slk, const bool optional_ints) {
	BinaryReader reader = hierarchy.map_file_read(file_name).value();

	const int version = reader.read<uint32_t>();
	if (version != 1 && version != 2 && version != 3) {
		std::cout << "Unknown modification table version of " << version << " detected. Attempting to load, but may crash.\n";
	}

	load_modification_table(reader, version, base_data, meta_slk, false, optional_ints);
	load_modification_table(reader, version, base_data, meta_slk, true, optional_ints);
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

		std::println("Meta data key not found for id {} property {}", id, property_id);
		exit(0);
	}
	const std::string meta_data_key = std::string(meta_id2.value());

	if (optional_ints) {
		const bool has_repeat = meta_slk.data<std::string_view>("repeat", meta_data_key) != "0";
		const bool append_index =
			meta_slk.column_headers.contains("appendindex") && meta_slk.data<int>("appendindex", meta_data_key) > 0;

		if (has_repeat && !append_index) {
			size_t nr_position = property_id.find_first_of("0123456789");
			if (nr_position != std::string::npos) {
				variation = std::stoi(property_id.substr(nr_position));
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

	int write_type = -1;
	const std::string_view type = meta_slk.data<std::string_view>("type", meta_data_key);
	if (type == "int" || type == "bool" || type.ends_with("Flags") || type == "attackBits" || type == "channelType" || type == "deathType" || type == "defenseTypeInt" || type == "detectionType" || type == "spellDetail" || type == "teamColor" || type == "techAvail") {
		write_type = 0;
	} else if (type == "real") {
		write_type = 1;
	} else if (type == "unreal") {
		write_type = 2;
	} else {
		write_type = 3;
	}

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

} // namespace

export void save_modification_table(BinaryWriter& writer, const slk::SLK& slk, const slk::SLK& meta_slk, const bool custom, const bool optional_ints, const bool skin) {
	BinaryWriter sub_writer;

	size_t count = 0;
	for (const auto& [id, properties] : slk.shadow_data) {
		if (!custom && properties.contains("oldid")) {
			continue;
		} else if (custom && !properties.contains("oldid")) {
			continue;
		}
		count++;

		if (custom) {
			sub_writer.write_string(properties.at("oldid"));
			sub_writer.write_string(id);
		} else {
			sub_writer.write_string(id);
			sub_writer.write<uint32_t>(0);
		}

		sub_writer.write<uint32_t>(1);
		sub_writer.write<uint32_t>(0);

		if (skin) {
			sub_writer.write<uint32_t>(0);
			continue;
		}
		sub_writer.write<uint32_t>(properties.size() - (properties.contains("oldid") ? 1 : 0));

		for (const auto& [property_id, value] : properties) {
			write_modification_property(sub_writer, slk, meta_slk, id, property_id, value, optional_ints, nullptr);
		}
	}

	writer.write<uint32_t>(count);
	writer.write_vector(sub_writer.buffer);
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
	BinaryWriter sub_writer;

	size_t count = 0;
	for (const auto& [id, properties] : slk.shadow_data) {
		if (!custom && properties.contains("oldid")) {
			continue;
		} else if (custom && !properties.contains("oldid")) {
			continue;
		}
		count++;

		if (custom) {
			sub_writer.write_string(properties.at("oldid"));
			sub_writer.write_string(id);
		} else {
			sub_writer.write_string(id);
			sub_writer.write<uint32_t>(0);
		}

		sub_writer.write<uint32_t>(1);
		sub_writer.write<uint32_t>(0);

		if (skin) {
			sub_writer.write<uint32_t>(0);
			continue;
		}

		size_t written_modifications = 0;
		BinaryWriter property_writer;

		for (const auto& [property_id, value] : properties) {
			const size_t before = property_writer.buffer.size();
			if (!write_modification_property(property_writer, slk, meta_slk, id, property_id, value, optional_ints, &errors)) {
				property_writer.buffer.resize(before);
				continue;
			}
			written_modifications++;
		}

		sub_writer.write<uint32_t>(written_modifications);
		sub_writer.buffer.insert(sub_writer.buffer.end(), property_writer.buffer.begin(), property_writer.buffer.end());
	}

	writer.write<uint32_t>(count);
	writer.write_vector(sub_writer.buffer);
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
	size_t count = 0;
	for (const auto& [id, properties] : slk.shadow_data) {
		(void)id;
		(void)properties;
		count++;
	}
	return count;
}

export bool modification_table_has_data(const slk::SLK& slk) {
	return !slk.shadow_data.empty();
}

export using ModificationShadowMap = hive::unordered_map<std::string, hive::unordered_map<std::string, std::string>>;

export std::expected<ModificationShadowMap, std::string> extract_modification_shadow_map(
	BinaryReader reader,
	const slk::SLK& template_slk,
	const slk::SLK& meta_slk,
	const bool optional_ints
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
			const std::string original_id = reader.read_string(4);
			const std::string modified_id = reader.read_string(4);

			if (*version >= 3) {
				[[maybe_unused]] const uint32_t set_count = reader.read<uint32_t>();
				[[maybe_unused]] const uint32_t set_flags = reader.read<uint32_t>();
			}

			const std::string& object_id = modification ? modified_id : original_id;
			if (modification) {
				shadow_map[object_id]["oldid"] = original_id;
			}

			const uint32_t modifications = reader.read<uint32_t>();
			if (modifications > 10000) {
				throw std::runtime_error(std::format("Too many modifications ({}) for object {}.", modifications, object_id));
			}

			for (size_t j = 0; j < modifications; j++) {
				const std::string modification_id = reader.read_string(4);
				const uint32_t type = reader.read<uint32_t>();

				std::string column_header = to_lowercase_copy(meta_slk.data<std::string_view>("field", modification_id));
				if (optional_ints) {
					const uint32_t level_variation = reader.read<uint32_t>();
					const uint32_t data_pointer = reader.read<uint32_t>();
					if (data_pointer != 0) {
						column_header += static_cast<char>('a' + data_pointer - 1);
					}
					if (meta_slk.data<std::string_view>("repeat", modification_id) != "0") {
						column_header += std::to_string(level_variation);
					}
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

				if (column_header.empty()) {
					continue;
				}

				shadow_map[object_id][column_header] = std::move(data);
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
	const bool optional_ints
) {
	const auto file = read_file(path);
	if (!file) {
		return std::unexpected(file.error());
	}
	return extract_modification_shadow_map(std::move(file.value()), template_slk, meta_slk, optional_ints);
}

export slk::SLK shadow_map_to_slk(const slk::SLK& template_slk, const ModificationShadowMap& shadow_map) {
	slk::SLK scratch;
	scratch.base_data = template_slk.base_data;
	scratch.row_headers = template_slk.row_headers;
	scratch.column_headers = template_slk.column_headers;
	scratch.index_to_row = template_slk.index_to_row;
	scratch.index_to_column = template_slk.index_to_column;

	for (const auto& [object_id, fields] : shadow_map) {
		if (fields.contains("oldid") && !scratch.row_headers.contains(object_id)) {
			const std::string& parent = fields.at("oldid");
			if (scratch.row_headers.contains(parent) || scratch.base_data.contains(parent)) {
				scratch.copy_row(parent, object_id, false);
			}
		}
		for (const auto& [field, value] : fields) {
			scratch.set_shadow_data(field, object_id, value);
		}
	}

	return scratch;
}
