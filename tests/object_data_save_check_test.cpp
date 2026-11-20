#include <doctest/doctest.h>

import std;
import types;
import no_init_allocator;
import BinaryReader;
import BinaryWriter;
import SLK;
import ModificationTables;
import Globals;
import Map;

namespace {

/// A stock-like ability meta table with the plain, non-repeating "Name" field (anam).
slk::SLK ability_name_meta() {
	slk::SLK meta;
	meta.add_row("anam");
	meta.set_shadow_data("field", "anam", "Name");
	meta.set_shadow_data("repeat", "anam", "0");
	meta.set_shadow_data("type", "anam", "string");
	meta.build_meta_map();
	return meta;
}

/// war3map.w3a with one modification of AHbz's anam, written with the given data pointer.
std::vector<u8, default_init_allocator<u8>> ability_name_file(const u32 data_pointer) {
	BinaryWriter writer;
	writer.write<u32>(3); // version
	writer.write<u32>(1); // original objects
	writer.write_string("AHbz");
	writer.write<u32>(0); // no modified id
	writer.write<u32>(1); // set count
	writer.write<u32>(0); // set flag
	writer.write<u32>(1); // modifications
	writer.write_string("anam");
	writer.write<u32>(3); // string
	writer.write<u32>(0); // level
	writer.write<u32>(data_pointer);
	writer.write_c_string("X");
	writer.write<u32>(0); // end token
	writer.write<u32>(0); // custom objects
	return { writer.buffer.begin(), writer.buffer.end() };
}

/// Loads the file into the global ability tables the way Map::load does, and restores them afterwards.
class LoadedAbilities {
	slk::SLK previous_data = abilities_slk;
	slk::SLK previous_meta = abilities_meta_slk;

  public:
	explicit LoadedAbilities(const u32 data_pointer) {
		abilities_meta_slk = ability_name_meta();
		abilities_slk = slk::SLK();
		abilities_slk.add_row("AHbz");
		abilities_slk.base_data["AHbz"]["code"] = "AHbz";

		BinaryReader reader(ability_name_file(data_pointer));
		REQUIRE(load_modification_file_reader(reader, abilities_slk, abilities_meta_slk, true).has_value());
	}
	~LoadedAbilities() {
		abilities_slk = previous_data;
		abilities_meta_slk = previous_meta;
	}
};

} // namespace

TEST_CASE("Map::save preflight finds an ability field the writer cannot write") {
	// Data pointer 1 on a field without data levels loads as column "namea", which no meta data row maps to.
	const LoadedAbilities loaded(1);
	REQUIRE(abilities_slk.shadow_data.at("AHbz").contains("namea"));

	const auto unwritable = find_unwritable_object_data();
	REQUIRE(unwritable.has_value());
	CHECK(unwritable->file_name == "war3map.w3a");
	REQUIRE(unwritable->errors.size() == 1);
	CHECK(unwritable->errors.front().object_id == "AHbz");
	CHECK(unwritable->errors.front().field == "namea");

	// Without the preflight the writer would stop the save halfway; it now throws instead of exiting.
	CHECK_THROWS_AS((void)build_modification_file_buffer(abilities_slk, abilities_meta_slk, true, false), std::runtime_error);
}

TEST_CASE("Map::save preflight passes object data the writer can write") {
	const LoadedAbilities loaded(0);
	REQUIRE(abilities_slk.shadow_data.at("AHbz").contains("name"));

	CHECK_FALSE(find_unwritable_object_data().has_value());
	CHECK_NOTHROW((void)build_modification_file_buffer(abilities_slk, abilities_meta_slk, true, false));
}

TEST_CASE("an unparsable level number is reported, not thrown, by the preflight") {
	slk::SLK meta;
	meta.add_row("Ocr1");
	meta.set_shadow_data("field", "Ocr1", "Data");
	meta.set_shadow_data("data", "Ocr1", "1");
	meta.set_shadow_data("repeat", "Ocr1", "1");
	meta.set_shadow_data("type", "Ocr1", "int");
	meta.build_meta_map();

	slk::SLK data;
	data.add_row("AOcr");
	data.base_data["AOcr"]["code"] = "AOcr";
	data.set_shadow_data("dataa99999999999999999999", "AOcr", "1");

	std::vector<ModificationSerializeError> errors;
	CHECK_NOTHROW(errors = collect_save_modification_file_errors(data, meta, true));
	REQUIRE(errors.size() == 1);
	CHECK(errors.front().field == "dataa99999999999999999999");
	CHECK_THROWS_AS((void)build_modification_file_buffer(data, meta, true, false), std::runtime_error);
}
