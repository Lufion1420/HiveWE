#define DOCTEST_CONFIG_USE_STD_HEADERS
#include <doctest/doctest.h>

import std;
import types;
import no_init_allocator;
import BinaryReader;
import BinaryWriter;
import SLK;
import ModificationTables;
import Hierarchy;
import AssetObfuscation;
import Globals;
import ObjectDataIo;
import Utilities;

using namespace std::string_literals;
namespace fs = std::filesystem;

// Ticket "[3.0] Update stale metadata overrides that drop object fields": a field id HiveWE's meta data does not know used to be dropped
// on load (w3u/w3t/w3b/w3h) or turned into a fake column such as "a1" (w3a/w3d/w3q), so the next save lost it or refused.

namespace {

struct Field {
	std::string id;
	u32 type = 0;
	u32 level = 0;
	u32 pointer = 0;
	std::vector<u8> value; // the value bytes as written, including a string's NUL
	u32 end = 0;
};

struct Object {
	std::string original;
	std::string modified;
	std::vector<Field> fields;
};

struct Table {
	std::vector<Object> objects;
};

struct File {
	Table original;
	Table custom;
};

std::vector<u8> int_bytes(const int value) {
	std::vector<u8> bytes(4);
	std::memcpy(bytes.data(), &value, 4);
	return bytes;
}

std::vector<u8> float_bytes(const float value) {
	std::vector<u8> bytes(4);
	std::memcpy(bytes.data(), &value, 4);
	return bytes;
}

std::vector<u8> string_bytes(const std::string& value) {
	std::vector<u8> bytes(value.begin(), value.end());
	bytes.push_back(0);
	return bytes;
}

Field int_field(const std::string& id, const int value, const u32 type = 0, const u32 level = 0, const u32 pointer = 0, const u32 end = 0) {
	return { id, type, level, pointer, int_bytes(value), end };
}

Field string_field(const std::string& id, const std::string& value, const u32 level = 0, const u32 pointer = 0, const u32 end = 0) {
	return { id, 3, level, pointer, string_bytes(value), end };
}

void write_table(BinaryWriter& writer, const Table& table, const bool optional_ints) {
	writer.write<u32>(static_cast<u32>(table.objects.size()));
	for (const Object& object : table.objects) {
		writer.write_string(object.original);
		if (object.modified.empty()) {
			writer.write<u32>(0);
		} else {
			writer.write_string(object.modified);
		}
		writer.write<u32>(1);
		writer.write<u32>(0);
		writer.write<u32>(static_cast<u32>(object.fields.size()));
		for (const Field& field : object.fields) {
			writer.write_string(field.id);
			writer.write<u32>(field.type);
			if (optional_ints) {
				writer.write<u32>(field.level);
				writer.write<u32>(field.pointer);
			}
			writer.write_vector(field.value);
			writer.write<u32>(field.end);
		}
	}
}

std::vector<u8> write_file(const File& file, const bool optional_ints) {
	BinaryWriter writer;
	writer.write<u32>(3);
	write_table(writer, file.original, optional_ints);
	write_table(writer, file.custom, optional_ints);
	return std::vector<u8>(writer.buffer.begin(), writer.buffer.end());
}

Table read_table(BinaryReader& reader, const bool optional_ints) {
	Table table;
	const u32 objects = reader.read<u32>();
	for (u32 i = 0; i < objects; i++) {
		Object object;
		object.original = reader.read_string(4);
		object.modified = reader.read_string(4);
		reader.advance(8);
		const u32 count = reader.read<u32>();
		for (u32 j = 0; j < count; j++) {
			Field field;
			field.id = reader.read_string(4);
			field.type = reader.read<u32>();
			if (optional_ints) {
				field.level = reader.read<u32>();
				field.pointer = reader.read<u32>();
			}
			if (field.type == 3) {
				const std::string text = reader.read_c_string();
				field.value = string_bytes(text);
			} else {
				const auto bytes = reader.read_vector<u8>(4);
				field.value.assign(bytes.begin(), bytes.end());
			}
			field.end = reader.read<u32>();
			object.fields.push_back(std::move(field));
		}
		table.objects.push_back(std::move(object));
	}
	return table;
}

File read_file_bytes(const std::vector<u8>& bytes, const bool optional_ints) {
	BinaryReader reader(std::vector<u8, default_init_allocator<u8>>(bytes.begin(), bytes.end()));
	REQUIRE(reader.read<u32>() == 3);
	File file;
	file.original = read_table(reader, optional_ints);
	file.custom = read_table(reader, optional_ints);
	CHECK(reader.remaining() == 0);
	return file;
}

const Field* find_field(const Table& table, const std::string& object_id, const std::string& field_id, const size_t nth = 0) {
	size_t seen = 0;
	for (const Object& object : table.objects) {
		const std::string& id = object.modified.empty() ? object.original : object.modified;
		if (id != object_id) {
			continue;
		}
		for (const Field& field : object.fields) {
			if (field.id == field_id && seen++ == nth) {
				return &field;
			}
		}
	}
	return nullptr;
}

void add_meta(slk::SLK& meta, const std::string& id, const std::string& field, const std::string& type, const std::string& repeat = {},
			  const std::string& data = {}) {
	meta.add_row(id);
	meta.set_shadow_data("field", id, field);
	meta.set_shadow_data("type", id, type);
	if (!repeat.empty()) {
		meta.set_shadow_data("repeat", id, repeat);
	}
	if (!data.empty()) {
		meta.set_shadow_data("data", id, data);
	}
}

/// A unit meta table that knows unam, uhpm and umdl but none of the 3.0 additions.
slk::SLK old_unit_meta() {
	slk::SLK meta;
	add_meta(meta, "unam", "Name", "string");
	add_meta(meta, "uhpm", "HP", "int");
	add_meta(meta, "umdl", "file", "model");
	meta.build_meta_map();
	return meta;
}

slk::SLK unit_table() {
	slk::SLK units;
	units.add_row("hfoo");
	units.base_data["hfoo"]["name"] = "Footman";
	units.base_data["hfoo"]["hp"] = "420";
	units.add_row("hkni");
	units.base_data["hkni"]["name"] = "Knight";
	return units;
}

/// An ability meta table with one levelled data field (Ocr1 -> dataa) and the name.
slk::SLK ability_meta() {
	slk::SLK meta;
	add_meta(meta, "anam", "Name", "string", "0");
	add_meta(meta, "Ocr1", "Data", "int", "1", "1");
	meta.set_shadow_data("usespecific", "Ocr1", "AOcr");
	meta.build_meta_map();
	return meta;
}

slk::SLK ability_table() {
	slk::SLK abilities;
	abilities.add_row("AOcr");
	abilities.base_data["AOcr"]["code"] = "AOcr";
	abilities.base_data["AOcr"]["name"] = "Critical Strike";
	return abilities;
}

void load(slk::SLK& table, const slk::SLK& meta, const std::vector<u8>& bytes, const bool optional_ints) {
	BinaryReader reader(std::vector<u8, default_init_allocator<u8>>(bytes.begin(), bytes.end()));
	REQUIRE(load_modification_file_reader(reader, table, meta, optional_ints).has_value());
}

std::string hex(const std::vector<u8>& bytes) {
	std::string text;
	for (const u8 byte : bytes) {
		text += std::format("{:02x}", byte);
	}
	return text;
}

} // namespace

TEST_CASE("unknown object data: a w3u keeps upor and an invented field with their type and value when the meta lacks them") {
	const slk::SLK meta = old_unit_meta();

	File file;
	file.original.objects.push_back({ "hfoo", "", { string_field("unam", "Big Footman"), string_field("upor", "war3mapImported\\Portrait.mdx"),
												  int_field("zzzz", 1234, 0, 0, 0, 0x6f6f6668) } });
	file.custom.objects.push_back({ "hkni", "h000", { int_field("zzzz", 0x3f800000, 1), string_field("upor", "Units\\Knight_portrait.mdx") } });
	const std::vector<u8> input = write_file(file, false);

	slk::SLK units = unit_table();
	load(units, meta, input, false);
	CHECK(units.data("name", "hfoo") == "Big Footman");
	CHECK(collect_save_modification_file_errors(units, meta, false).empty());

	const File saved = read_file_bytes(build_modification_file_buffer(units, meta, false, false), false);

	const Field* upor = find_field(saved.original, "hfoo", "upor");
	REQUIRE(upor != nullptr);
	CHECK(upor->type == 3);
	CHECK(upor->value == string_bytes("war3mapImported\\Portrait.mdx"));

	const Field* invented = find_field(saved.original, "hfoo", "zzzz");
	REQUIRE(invented != nullptr);
	CHECK(invented->type == 0);
	CHECK(invented->value == int_bytes(1234));
	CHECK(invented->end == 0x6f6f6668); // the end token as read

	const Field* custom_real = find_field(saved.custom, "h000", "zzzz");
	REQUIRE(custom_real != nullptr);
	CHECK(custom_real->type == 1);
	CHECK(custom_real->value == int_bytes(0x3f800000));
	REQUIRE(find_field(saved.custom, "h000", "upor") != nullptr);
	CHECK(find_field(saved.custom, "h000", "upor")->value == string_bytes("Units\\Knight_portrait.mdx"));
}

TEST_CASE("unknown object data: a w3a level field with an unknown id makes no fake column and no save error") {
	const slk::SLK meta = ability_meta();

	File file;
	file.custom.objects.push_back(
		{ "AOcr", "A000", { int_field("Ocr1", 25, 0, 1, 1), int_field("Zqq1", 7, 0, 1, 1), int_field("Zqq2", 9, 0, 1, 1), string_field("Zstr", "x", 2, 0) } }
	);
	const std::vector<u8> input = write_file(file, true);

	slk::SLK abilities = ability_table();
	const size_t columns_before = abilities.columns();
	load(abilities, meta, input, true);

	CHECK(abilities.data("dataa1", "A000") == "25");
	CHECK_FALSE(abilities.column_headers.contains("a1"));
	CHECK_FALSE(abilities.column_headers.contains("2"));
	CHECK(abilities.columns() == columns_before + 1); // only dataa1
	CHECK(collect_save_modification_file_errors(abilities, meta, true).empty());

	const File saved = read_file_bytes(build_modification_file_buffer(abilities, meta, true, false), true);
	// Two unknown ids at the same level and data pointer both survive.
	const Field* first = find_field(saved.custom, "A000", "Zqq1");
	const Field* second = find_field(saved.custom, "A000", "Zqq2");
	REQUIRE(first != nullptr);
	REQUIRE(second != nullptr);
	CHECK(first->level == 1);
	CHECK(first->pointer == 1);
	CHECK(first->value == int_bytes(7));
	CHECK(second->value == int_bytes(9));
	const Field* text = find_field(saved.custom, "A000", "Zstr");
	REQUIRE(text != nullptr);
	CHECK(text->level == 2);
	CHECK(text->value == string_bytes("x"));
	REQUIRE(find_field(saved.custom, "A000", "Ocr1") != nullptr);
}

TEST_CASE("unknown object data: saved bytes are unchanged for data without unknown ids (golden, from the build before the change)") {
	// Units: no optional ints, an original and a custom object, int/real/unreal/string/bool types.
	slk::SLK unit_meta;
	add_meta(unit_meta, "unam", "Name", "string");
	add_meta(unit_meta, "uhpm", "HP", "int");
	add_meta(unit_meta, "usca", "modelScale", "real");
	add_meta(unit_meta, "uaen", "acquire", "unreal");
	add_meta(unit_meta, "usid", "sightRad", "bool");
	add_meta(unit_meta, "umdl", "file", "model");
	unit_meta.build_meta_map();

	slk::SLK units = unit_table();
	units.set_shadow_data("name", "hfoo", "Golden Footman");
	units.set_shadow_data("hp", "hfoo", "777");
	units.set_shadow_data("modelscale", "hfoo", "1.250000");
	units.copy_row("hkni", "h001", false);
	units.set_shadow_data("name", "h001", "Golden Knight");
	units.set_shadow_data("acquire", "h001", "-2.500000");
	units.set_shadow_data("sightrad", "h001", "1");
	units.set_shadow_data("file", "h001", "war3mapImported\\Knight.mdx");

	// Abilities: optional ints, a levelled data field with a data pointer.
	const slk::SLK abil_meta = ability_meta();
	slk::SLK abilities = ability_table();
	abilities.set_shadow_data("name", "AOcr", "Golden Strike");
	abilities.copy_row("AOcr", "A001", false);
	abilities.set_shadow_data("dataa1", "A001", "15");
	abilities.set_shadow_data("dataa2", "A001", "30");

	const std::string units_main = hex(build_modification_file_buffer(units, unit_meta, false, false));
	const std::string units_skin = hex(build_modification_file_buffer(units, unit_meta, false, true));
	const std::string abilities_main = hex(build_modification_file_buffer(abilities, abil_meta, true, false));
	const std::string abilities_skin = hex(build_modification_file_buffer(abilities, abil_meta, true, true));

	// Captured from the build at 82fb258 (before unknown fields were kept).
	CHECK(units_main == "030000000100000068666f6f00000000010000000000000003000000756e616d03000000476f6c64656e20466f6f746d616e00000000007568706d00000000090300000000000075736361010000000000a03f0000000001000000686b6e6968303031010000000000000004000000756e616d03000000476f6c64656e204b6e6967687400000000007561656e02000000000020c00000000075736964000000000100000000000000756d646c03000000776172336d6170496d706f727465645c4b6e696768742e6d64780000000000");
	CHECK(units_skin == "030000000100000068666f6f0000000001000000000000000000000001000000686b6e6968303031010000000000000000000000");
	CHECK(abilities_main == "0300000001000000414f637200000000010000000000000001000000616e616d030000000000000000000000476f6c64656e20537472696b65000000000001000000414f6372413030310100000000000000020000004f6372310000000001000000010000000f000000000000004f6372310000000002000000010000001e00000000000000");
	CHECK(abilities_skin == "0300000001000000414f63720000000001000000000000000000000001000000414f637241303031010000000000000000000000");
}

TEST_CASE("unknown object data: skin file fields go back to the skin file, and an object with only unknown fields is written") {
	const slk::SLK meta = old_unit_meta();

	File main_file;
	main_file.original.objects.push_back({ "hkni", "", { int_field("zmai", 5) } }); // only an unknown field
	File skin_file;
	skin_file.original.objects.push_back({ "hfoo", "", { string_field("zski", "skin value"), string_field("unam", "Skin Name") } });

	slk::SLK units = unit_table();
	load(units, meta, write_file(main_file, false), false);
	{
		const std::vector<u8> bytes = write_file(skin_file, false);
		BinaryReader reader(std::vector<u8, default_init_allocator<u8>>(bytes.begin(), bytes.end()));
		REQUIRE(load_modification_file_reader(reader, units, meta, false, nullptr, true).has_value());
	}

	CHECK_FALSE(units.shadow_data.contains("hkni"));
	CHECK(count_modification_objects(units) == 2);
	CHECK(modification_table_has_data(units));

	const File main_saved = read_file_bytes(build_modification_file_buffer(units, meta, false, false), false);
	const File skin_saved = read_file_bytes(build_modification_file_buffer(units, meta, false, true), false);

	REQUIRE(find_field(main_saved.original, "hkni", "zmai") != nullptr);
	CHECK(find_field(main_saved.original, "hkni", "zmai")->value == int_bytes(5));
	CHECK(find_field(main_saved.original, "hfoo", "zski") == nullptr);
	// Known skin fields keep moving to the main file, as before.
	CHECK(find_field(main_saved.original, "hfoo", "unam") != nullptr);

	REQUIRE(find_field(skin_saved.original, "hfoo", "zski") != nullptr);
	CHECK(find_field(skin_saved.original, "hfoo", "zski")->value == string_bytes("skin value"));
	CHECK(find_field(skin_saved.original, "hkni", "zmai") == nullptr);
	CHECK(find_field(skin_saved.original, "hfoo", "unam") == nullptr);
}

TEST_CASE("unknown object data: delete drops, duplicate copies and the unsaved check counts the kept fields") {
	const slk::SLK meta = old_unit_meta();
	File file;
	file.custom.objects.push_back({ "hfoo", "h000", { string_field("unam", "Custom"), int_field("zzzz", 3) } });

	slk::SLK units = unit_table();
	load(units, meta, write_file(file, false), false);
	REQUIRE(units.raw_modifications.contains("h000"));

	// A copy of the freshly loaded table compares equal, so a just loaded map does not read as unsaved.
	const slk::SLK fresh = units;
	CHECK(fresh.serialized_shadow_data() == units.serialized_shadow_data());

	// The unsaved check sees a change to the kept fields alone.
	slk::SLK without_raw = units;
	without_raw.raw_modifications.clear();
	CHECK(without_raw.serialized_shadow_data() != units.serialized_shadow_data());

	units.copy_row("h000", "h001", true); // Object Editor duplicate and paste
	REQUIRE(units.raw_modifications.contains("h001"));
	CHECK(units.raw_modifications.at("h001") == units.raw_modifications.at("h000"));
	const File saved = read_file_bytes(build_modification_file_buffer(units, meta, false, false), false);
	CHECK(find_field(saved.custom, "h001", "zzzz") != nullptr);
	CHECK(find_field(saved.custom, "h000", "zzzz") != nullptr);

	units.remove_row("h000"); // Object Editor delete
	CHECK_FALSE(units.raw_modifications.contains("h000"));
	const File after_delete = read_file_bytes(build_modification_file_buffer(units, meta, false, false), false);
	CHECK(find_field(after_delete.custom, "h000", "zzzz") == nullptr);
	CHECK(find_field(after_delete.custom, "h001", "zzzz") != nullptr);
}

TEST_CASE("unknown object data: the type byte of an unverified 3.0 type is kept until the value is edited") {
	// Pins the writer's type bytes; switching the default for the 3.0 types is a change here.
	CHECK(modification_write_type("int") == 0);
	CHECK(modification_write_type("bool") == 0);
	CHECK(modification_write_type("teamColor") == 0);
	CHECK(modification_write_type("real") == 1);
	CHECK(modification_write_type("unreal") == 2);
	CHECK(modification_write_type("string") == 3);
	CHECK(modification_write_type("model") == 3);
	CHECK(modification_write_type("equipmentType") == 3);
	CHECK(modification_write_type("itemTag") == 3);
	CHECK(modification_write_type("occlusionType") == 3);
	CHECK(is_unverified_write_type("occlusionType"));
	CHECK_FALSE(is_unverified_write_type("string"));

	slk::SLK meta = old_unit_meta();
	add_meta(meta, "uoct", "occlusion", "occlusionType");
	meta.build_meta_map();

	File file;
	file.original.objects.push_back({ "hfoo", "", { int_field("uoct", 2, 0, 0, 0, 0x6f6f6668) } }); // written as an int
	file.original.objects.push_back({ "hkni", "", { string_field("uoct", "Occluder") } });         // written as the default
	slk::SLK units = unit_table();
	load(units, meta, write_file(file, false), false);
	CHECK(units.data("occlusion", "hfoo") == "2");
	CHECK(units.data("occlusion", "hkni") == "Occluder");
	CHECK(units.raw_modifications.contains("hfoo"));
	CHECK_FALSE(units.raw_modifications.contains("hkni"));

	const File saved = read_file_bytes(build_modification_file_buffer(units, meta, false, false), false);
	REQUIRE(find_field(saved.original, "hfoo", "uoct") != nullptr);
	CHECK(find_field(saved.original, "hfoo", "uoct")->type == 0);
	CHECK(find_field(saved.original, "hfoo", "uoct")->value == int_bytes(2));
	CHECK(find_field(saved.original, "hfoo", "uoct")->end == 0x6f6f6668);
	CHECK(find_field(saved.original, "hfoo", "uoct", 1) == nullptr); // written once
	CHECK(find_field(saved.original, "hkni", "uoct")->type == 3);

	units.set_shadow_data("occlusion", "hfoo", "Occludee"); // edited in HiveWE: the default byte
	const File edited = read_file_bytes(build_modification_file_buffer(units, meta, false, false), false);
	REQUIRE(find_field(edited.original, "hfoo", "uoct") != nullptr);
	CHECK(find_field(edited.original, "hfoo", "uoct")->type == 3);
	CHECK(find_field(edited.original, "hfoo", "uoct")->value == string_bytes("Occludee"));
	CHECK(find_field(edited.original, "hfoo", "uoct", 1) == nullptr);
}

TEST_CASE("unknown object data: the override meta SLKs hold the 3.0 rows with unique lowercase fields") {
	const fs::path overrides = OVERRIDES_DIR;
	struct Expected {
		std::string file;
		std::vector<std::string> ids;
		bool all_fields_unique;
	};
	const std::vector<Expected> expected = {
		{ "Units/UnitMetaData.slk", { "uatg", "uepa", "uevi", "ufhp", "umd1", "umd2", "uoct", "upor" }, true },
		{ "Units/ItemMetaData.slk", { "iequ", "iinv", "itag", "itcc", "itco", "unsf" }, true },
		// The stock misc table already repeats some fields (told apart by index), so only the new rows are checked there.
		{ "Units/MiscMetaData.slk", { "bmcd", "bscd", "cfha", "cizd", "ighb", "igmr" }, false },
	};

	size_t checked = 0;
	for (const Expected& entry : expected) {
		const slk::SLK meta(overrides / entry.file, true);
		REQUIRE_MESSAGE(meta.rows() > 0, entry.file);

		std::map<std::string, std::vector<std::string>> rows_by_field;
		for (const auto& [id, index] : meta.row_headers) {
			rows_by_field[to_lowercase_copy(meta.data<std::string>("field", id))].push_back(id);
		}
		for (const std::string& id : entry.ids) {
			REQUIRE_MESSAGE(meta.row_headers.contains(id), entry.file, " lacks ", id);
			const std::string field = to_lowercase_copy(meta.data<std::string>("field", id));
			CHECK_MESSAGE(!field.empty(), id);
			CHECK_MESSAGE(!meta.data<std::string>("type", id).empty(), id);
			CHECK_MESSAGE(rows_by_field.at(field).size() == 1, id, " shares field ", field);
			checked++;
		}
		if (entry.all_fields_unique) {
			for (const auto& [field, ids] : rows_by_field) {
				CHECK_MESSAGE(ids.size() == 1, entry.file, ": field ", field, " is used by ", ids.size(), " rows");
			}
		}
	}
	CHECK(checked == 20);

	// umd1/umd2 must not resolve through umdl's "file" (the meta map is a hash map, so a shared field made the lookup random).
	slk::SLK units_meta(overrides / "Units/UnitMetaData.slk", true);
	units_meta.build_meta_map();
	const slk::SLK units;
	CHECK(units.field_to_meta_id(units_meta, "file", "hfoo") == "umdl");
	CHECK(units.field_to_meta_id(units_meta, "file1", "hfoo") == "umd1");
	CHECK(units.field_to_meta_id(units_meta, "file2", "hfoo") == "umd2");
	CHECK(units.field_to_meta_id(units_meta, "portrait", "hfoo") == "upor");
	CHECK(units_meta.data<std::string>("type", "upor") == "model");
	CHECK(units_meta.data<std::string>("type", "umd1") == "model");
	CHECK(units_meta.data<std::string>("type", "umd2") == "model");
}

TEST_CASE("unknown object data: Map Protector keeps an unknown field next to a renamed path and renames upor and umd1") {
	const fs::path dir = fs::temp_directory_path() / "hivewe_unknown_fields_test" / "protector";
	std::error_code ec;
	fs::remove_all(dir, ec);
	fs::create_directories(dir);
	const fs::path original_map_directory = hierarchy.map_directory;
	hierarchy.map_directory = dir;

	slk::SLK meta = old_unit_meta();
	add_meta(meta, "upor", "portrait", "model");
	add_meta(meta, "umd1", "file1", "model");
	meta.build_meta_map();
	const slk::SLK template_slk = unit_table();

	File file;
	file.original.objects.push_back({ "hfoo", "", {
		string_field("umdl", "war3mapImported\\Custom.mdx"),
		string_field("upor", "war3mapImported\\Custom.mdx"),
		string_field("umd1", "war3mapImported\\Custom.mdx"),
		int_field("zzzz", 42, 0, 0, 0, 0x6f6f6668),
		string_field("zpth", "war3mapImported\\Custom.mdx,Units\\Stock.mdx", 0, 0, 0x11223344),
	} });
	file.custom.objects.push_back({ "hkni", "h000", { string_field("zcus", "keep me") } });
	const fs::path w3u_path = dir / "war3map.w3u";
	{
		const std::vector<u8> bytes = write_file(file, false);
		std::ofstream out(w3u_path, std::ios::binary);
		out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}

	RenameCandidate candidate;
	candidate.original_relative_path = "war3mapImported/Custom.mdx";
	candidate.new_relative_path = "a3f9c1e2.mdx";
	candidate.match_key = asset_match_key("war3mapImported/Custom.mdx");
	const std::unordered_map<std::string, const RenameCandidate*> candidates_by_match_key = { { candidate.match_key, &candidate } };

	const FileRewriteResult result = rewrite_object_data_file(w3u_path, "war3map.w3u", template_slk, meta, false, false, candidates_by_match_key);
	CHECK(result.success);
	CHECK(result.changed);

	std::vector<u8> rewritten;
	{
		std::ifstream in(w3u_path, std::ios::binary);
		rewritten.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}
	const File saved = read_file_bytes(rewritten, false);
	for (const std::string id : { "umdl", "upor", "umd1" }) {
		REQUIRE_MESSAGE(find_field(saved.original, "hfoo", id) != nullptr, id);
		CHECK_MESSAGE(find_field(saved.original, "hfoo", id)->value == string_bytes("a3f9c1e2.mdx"), id);
	}
	REQUIRE(find_field(saved.original, "hfoo", "zzzz") != nullptr);
	CHECK(find_field(saved.original, "hfoo", "zzzz")->value == int_bytes(42));
	CHECK(find_field(saved.original, "hfoo", "zzzz")->end == 0x6f6f6668);
	REQUIRE(find_field(saved.original, "hfoo", "zpth") != nullptr);
	CHECK(find_field(saved.original, "hfoo", "zpth")->value == string_bytes("a3f9c1e2.mdx,Units\\Stock.mdx"));
	CHECK(find_field(saved.original, "hfoo", "zpth")->end == 0x11223344);
	REQUIRE(find_field(saved.custom, "h000", "zcus") != nullptr);
	CHECK(find_field(saved.custom, "h000", "zcus")->value == string_bytes("keep me"));

	hierarchy.map_directory = original_map_directory;
	fs::remove_all(dir, ec);
}

TEST_CASE("unknown object data: a binary object data import carries unknown fields of the main and skin files") {
	const fs::path dir = fs::temp_directory_path() / "hivewe_unknown_fields_test" / "import";
	std::error_code ec;
	fs::remove_all(dir, ec);
	fs::create_directories(dir);

	const slk::SLK saved_units = units_slk;
	const slk::SLK saved_meta = units_meta_slk;
	units_slk = unit_table();
	units_meta_slk = old_unit_meta();

	File main_file;
	main_file.original.objects.push_back({ "hkni", "", { int_field("zmai", 5) } });
	main_file.custom.objects.push_back({ "hfoo", "h000", { string_field("unam", "Imported"), string_field("zcus", "x") } });
	File skin_file;
	skin_file.original.objects.push_back({ "hfoo", "", { string_field("zski", "skin") } });
	for (const auto& [name, file] : { std::pair{ "war3map.w3u", &main_file }, std::pair{ "war3mapSkin.w3u", &skin_file } }) {
		const std::vector<u8> bytes = write_file(*file, false);
		std::ofstream out(dir / name, std::ios::binary);
		out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}

	const auto package = load_binary_object_data(dir, false);
	REQUIRE(package.has_value());
	REQUIRE(package->raw_categories.contains(ObjectDataCategory::unit));
	const slk::RawModificationMap& raw = package->raw_categories.at(ObjectDataCategory::unit);
	REQUIRE(raw.contains("hkni"));
	CHECK(raw.at("hkni").front().field_id == "zmai");
	CHECK_FALSE(raw.at("hkni").front().skin);
	REQUIRE(raw.contains("h000"));
	REQUIRE(raw.contains("hfoo"));
	CHECK(raw.at("hfoo").front().skin);
	CHECK(package->categories.at(ObjectDataCategory::unit).contains("hkni")); // an object with only unknown fields is listed

	const ObjectDataApplyPlan plan = build_import_plan(*package, {});
	REQUIRE(plan.raw.contains(ObjectDataCategory::unit));
	CHECK(plan.raw.at(ObjectDataCategory::unit).size() == 3);

	// An object the map already has is left alone unless it is overwritten.
	units_slk.set_shadow_data("name", "hkni", "Mine");
	const ObjectDataApplyPlan keep = build_import_plan(*package, {});
	CHECK_FALSE(keep.raw.at(ObjectDataCategory::unit).contains("hkni"));
	const ObjectDataApplyPlan overwrite = build_import_plan(*package, { "hkni" });
	CHECK(overwrite.raw.at(ObjectDataCategory::unit).contains("hkni"));

	units_slk = saved_units;
	units_meta_slk = saved_meta;
	fs::remove_all(dir, ec);
}

namespace {

slk::RawModification raw_record(const Field& field, const bool optional_ints, const std::string& column = {}, const bool skin = false) {
	BinaryWriter writer;
	writer.write_string(field.id);
	writer.write<u32>(field.type);
	if (optional_ints) {
		writer.write<u32>(field.level);
		writer.write<u32>(field.pointer);
	}
	writer.write_vector(field.value);
	writer.write<u32>(field.end);

	slk::RawModification record;
	record.field_id = field.id;
	record.type = field.type;
	record.level = field.level;
	record.pointer = field.pointer;
	record.column = column;
	record.skin = skin;
	record.bytes.assign(writer.buffer.begin(), writer.buffer.end());
	if (field.type == 3) {
		record.value = std::string(field.value.begin(), field.value.end() - 1);
	} else {
		int value = 0;
		std::memcpy(&value, field.value.data(), 4);
		record.value = std::to_string(value);
	}
	return record;
}

size_t field_count(const Table& table, const std::string& object_id, const std::string& field_id) {
	size_t count = 0;
	while (find_field(table, object_id, field_id, count)) {
		count++;
	}
	return count;
}

ObjectCategoryDescriptor unit_descriptor(slk::SLK& units, slk::SLK& meta) {
	return { ObjectDataCategory::unit, "unit", "Unit", "war3map.w3u", "war3mapSkin.w3u", "unit.ini", &units, &meta, nullptr, false };
}

} // namespace

TEST_CASE("unknown object data: an import over an existing object replaces its unknown fields and writes no field twice") {
	slk::SLK meta = old_unit_meta();
	add_meta(meta, "uoct", "occlusion", "occlusionType");
	meta.build_meta_map();

	File file;
	file.original.objects.push_back({ "hfoo", "", { int_field("zzzz", 1), int_field("uoct", 2), int_field("zold", 5) } });
	slk::SLK units = unit_table();
	load(units, meta, write_file(file, false), false);
	REQUIRE(units.raw_modifications.at("hfoo").size() == 3);

	// The package brings the same unknown id with another value, and the pinned 3.0 field with another value read with the same byte.
	const std::vector<slk::RawModification> imported = { raw_record(int_field("zzzz", 9), false),
														 raw_record(int_field("uoct", 3), false, "occlusion") };
	merge_imported_raw_records(units.raw_modifications.at("hfoo"), imported, true);
	units.set_shadow_data("occlusion", "hfoo", "3"); // apply sets the imported known value too

	const File saved = read_file_bytes(build_modification_file_buffer(units, meta, false, false), false);
	CHECK(field_count(saved.original, "hfoo", "zzzz") == 1);
	CHECK(find_field(saved.original, "hfoo", "zzzz")->value == int_bytes(9));
	CHECK(field_count(saved.original, "hfoo", "zold") == 0); // overwritten: the map's own unknown fields go
	REQUIRE(field_count(saved.original, "hfoo", "uoct") == 1);
	CHECK(find_field(saved.original, "hfoo", "uoct")->type == 0);
	CHECK(find_field(saved.original, "hfoo", "uoct")->value == int_bytes(3));

	// A pinned record the import does not replace stays when the object is overwritten.
	slk::SLK again = unit_table();
	load(again, meta, write_file(file, false), false);
	merge_imported_raw_records(again.raw_modifications.at("hfoo"), { raw_record(int_field("znew", 4), false) }, true);
	const File kept = read_file_bytes(build_modification_file_buffer(again, meta, false, false), false);
	CHECK(field_count(kept.original, "hfoo", "uoct") == 1);
	CHECK(find_field(kept.original, "hfoo", "uoct")->type == 0);
	CHECK(field_count(kept.original, "hfoo", "zzzz") == 0);
	CHECK(field_count(kept.original, "hfoo", "znew") == 1);

	// A new object made by a row copy brings its source's records; the imported record of the same field replaces the copied one.
	units.copy_row("hfoo", "h000", true);
	merge_imported_raw_records(units.raw_modifications.at("h000"), { raw_record(int_field("zzzz", 7), false) }, false);
	const File copied = read_file_bytes(build_modification_file_buffer(units, meta, false, false), false);
	CHECK(field_count(copied.custom, "h000", "zzzz") == 1);
	CHECK(find_field(copied.custom, "h000", "zzzz")->value == int_bytes(7));

	// Levelled tables: the same id at another level or data pointer is another field.
	std::vector<slk::RawModification> levels = { raw_record(int_field("Zqq1", 1, 0, 1, 1), true) };
	merge_imported_raw_records(levels, { raw_record(int_field("Zqq1", 2, 0, 2, 1), true), raw_record(int_field("Zqq1", 3, 0, 1, 1), true) }, false);
	REQUIRE(levels.size() == 2);
	CHECK(std::ranges::count_if(levels, [](const slk::RawModification& r) { return r.level == 1 && r.value == "3"; }) == 1);
	CHECK(std::ranges::count_if(levels, [](const slk::RawModification& r) { return r.level == 2; }) == 1);
}

TEST_CASE("unknown object data: an import conflicts when an object differs only in unknown fields") {
	slk::SLK meta = old_unit_meta();
	slk::SLK units = unit_table();
	File file;
	file.original.objects.push_back({ "hfoo", "", { string_field("unam", "Same"), int_field("zzzz", 1) } });
	file.original.objects.push_back({ "hkni", "", { int_field("zonl", 2) } }); // only an unknown field
	load(units, meta, write_file(file, false), false);
	const ObjectCategoryDescriptor descriptor = unit_descriptor(units, meta);

	ModificationShadowMap imported;
	imported["hfoo"]["name"] = "Same";
	(void)imported["hkni"];

	// The package lacks the map's unknown fields (or has other ones): both objects conflict.
	const auto without_raw = detect_import_conflicts(descriptor, imported);
	CHECK(without_raw.size() == 2);

	slk::RawModificationMap other;
	other["hfoo"].push_back(raw_record(int_field("zzzz", 9), false));
	other["hkni"].push_back(raw_record(int_field("zonl", 2), false));
	const auto with_other = detect_import_conflicts(descriptor, imported, &other);
	REQUIRE(with_other.size() == 1);
	CHECK(with_other.front().id == "hfoo");

	// The same fields in another order are not a conflict.
	slk::RawModificationMap same;
	same["hfoo"].push_back(raw_record(int_field("zzzz", 1), false));
	same["hkni"].push_back(raw_record(int_field("zonl", 2), false));
	CHECK(detect_import_conflicts(descriptor, imported, &same).empty());
	CHECK(detect_import_additions(descriptor, imported).empty()); // the map already has hkni
}
