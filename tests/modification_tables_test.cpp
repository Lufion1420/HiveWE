#include <doctest/doctest.h>

import std;
import types;
import no_init_allocator;
import BinaryReader;
import BinaryWriter;
import SLK;
import ModificationTables;

using namespace std::string_literals;

constexpr u32 mod_table_version = 3;

TEST_CASE("save_modification_table data repeat") {
	slk::SLK meta;
	meta.add_row("Ocr6");
	meta.set_shadow_data("field", "Ocr6", "data");
	meta.set_shadow_data("repeat", "Ocr6", "4"); // non-zero indicates repeating
	meta.set_shadow_data("data", "Ocr6", "1");
	meta.set_shadow_data("type", "Ocr6", "int");
	meta.build_meta_map();

	slk::SLK data;
	data.add_row("Test");
	data.set_shadow_data("dataa1", "Test", "67");

	BinaryWriter writer;
	save_modification_table(writer, data, meta, false, true, false);

	slk::SLK loaded;
	BinaryReader reader(writer.buffer);
	load_modification_table(reader, mod_table_version, loaded, meta, false, true);

	CHECK(loaded.data("dataa1", "Test") == "67");
}

// Asana 1220369536243160: stock campaign map humanx03 holds a leftover custom unit n003 on a base (nbsh) no 3.0.1 table has. The load used
// to throw (Asana 1220330336212700 made that message name both ids); it now skips the object and reports it.
TEST_CASE("a custom object based on an id the table lacks is skipped and reported, others stay intact") {
	slk::SLK meta;
	meta.add_row("unam");
	meta.set_shadow_data("field", "unam", "Name");
	meta.set_shadow_data("type", "unam", "string");
	meta.build_meta_map();

	const auto write_object = [](BinaryWriter& writer, const std::string& original, const std::string& modified, const std::string& name) {
		writer.write_string(original);
		writer.write_string(modified);
		writer.write<u32>(1); // set count
		writer.write<u32>(0); // set flag
		writer.write<u32>(1); // one modification
		writer.write_string("unam");
		writer.write<u32>(3); // string
		writer.write_c_string(name);
		writer.write<u32>(0); // end token
	};

	BinaryWriter writer;
	writer.write<u32>(3); // version
	writer.write<u32>(0); // original table: no objects
	writer.write<u32>(3); // custom table
	write_object(writer, "nbsh", "n003", "Leftover");
	write_object(writer, "hfoo", "h001", "Good Footman");
	write_object(writer, "nbsh", "n003", "Leftover again"); // the same id twice is reported once

	slk::SLK units;
	units.add_row("hfoo");
	units.base_data["hfoo"]["name"] = "Footman";

	std::vector<SkippedObject> skipped;
	BinaryReader reader(writer.buffer);
	REQUIRE(load_modification_file_reader(reader, units, meta, false, &skipped).has_value());

	REQUIRE(skipped.size() == 1);
	CHECK(skipped.front().id == "n003");
	CHECK(skipped.front().base_id == "nbsh");
	CHECK_FALSE(units.base_data.contains("n003"));
	CHECK_FALSE(units.row_headers.contains("n003"));
	CHECK_FALSE(units.shadow_data.contains("n003"));
	CHECK(units.shadow_data.contains("h001"));
	CHECK(units.data("name", "h001") == "Good Footman");
	CHECK(units.data("name", "hfoo") == "Footman");
	CHECK(collect_save_modification_file_errors(units, meta, false).empty());

	// The skin pass meets the same object again: still skipped, still reported once, nothing left behind.
	BinaryReader again(writer.buffer);
	REQUIRE(load_modification_file_reader(again, units, meta, false, &skipped).has_value());
	CHECK(skipped.size() == 1);
	CHECK_FALSE(units.shadow_data.contains("n003"));
}

TEST_CASE("shadow_map_to_slk does not throw when a custom object's oldid parent has no base_data entry") {
	// add_row() only populates row_headers/index_to_row, never base_data - a row can legitimately
	// exist in one without the other (e.g. a header declared with no cell data ever recorded for
	// it). copy_row() unconditionally does base_data.at(row_header), so a guard that treats "present
	// in row_headers" as good enough previously let this throw std::out_of_range instead of just
	// skipping the copy - found via a real crash reproducing this exact shape of data.
	slk::SLK template_slk;
	template_slk.add_row("ParentRow");

	ModificationShadowMap shadow_map;
	shadow_map["Cust"]["oldid"] = "ParentRow";
	shadow_map["Cust"]["name"] = "Custom Object";

	slk::SLK scratch;
	CHECK_NOTHROW(scratch = shadow_map_to_slk(template_slk, shadow_map));
	CHECK(scratch.data("name", "Cust") == "Custom Object");
}

TEST_CASE("shadow_map_to_slk copies base_data fields from a custom object's oldid parent when present") {
	slk::SLK template_slk;
	template_slk.add_row("Parent");
	template_slk.base_data["Parent"]["hp"] = "500";

	ModificationShadowMap shadow_map;
	shadow_map["Cust"]["oldid"] = "Parent";
	shadow_map["Cust"]["name"] = "Custom Object";

	const slk::SLK scratch = shadow_map_to_slk(template_slk, shadow_map);
	CHECK(scratch.base_data.contains("Cust"));
	CHECK(scratch.base_data.at("Cust").contains("hp"));
	CHECK(scratch.data("hp", "Cust") == "500"); // inherited from the parent via copy_row()
	CHECK(scratch.data("name", "Cust") == "Custom Object");
}

namespace {

/// An ability meta table whose "dataa" field only resolves through the ability's code (useSpecific), like most stock ability data fields.
slk::SLK use_specific_ability_meta() {
	slk::SLK meta;
	meta.add_row("Ocr1");
	meta.set_shadow_data("field", "Ocr1", "Data");
	meta.set_shadow_data("data", "Ocr1", "1");
	meta.set_shadow_data("repeat", "Ocr1", "1");
	meta.set_shadow_data("usespecific", "Ocr1", "AOcr");
	meta.set_shadow_data("type", "Ocr1", "int");
	meta.build_meta_map();
	return meta;
}

} // namespace

// NCOW_Spell_Dev crash (2026-11-20): a build with the old copy_row() copied an empty row for one custom ability while loading
// war3map.w3a, so the ability lost its "code", its useSpecific fields no longer resolved, and the save called exit(0) halfway.
TEST_CASE("custom abilities loaded from a modification table keep their parent's code and save again") {
	const slk::SLK meta = use_specific_ability_meta();

	// Enough stock rows and custom objects that copy_row() crosses several storage reallocations of base_data.
	const auto make_stock = [] {
		slk::SLK stock;
		for (int i = 0; i < 300; i++) {
			const std::string id = std::format("S{:03}", i);
			stock.add_row(id);
			stock.base_data[id]["code"] = id;
		}
		stock.add_row("AOcr");
		stock.base_data["AOcr"]["code"] = "AOcr";
		return stock;
	};

	slk::SLK source = make_stock();
	std::vector<std::string> custom_ids;
	for (int i = 0; i < 200; i++) {
		const std::string id = std::format("Z{:03}", i);
		custom_ids.push_back(id);
		source.copy_row("AOcr", id, false);
		source.set_shadow_data("dataa1", id, std::to_string(i));
	}

	const std::vector<u8> buffer = build_modification_file_buffer(source, meta, true, false);

	slk::SLK loaded = make_stock();
	BinaryReader reader(std::vector<u8, default_init_allocator<u8>>(buffer.begin(), buffer.end()));
	REQUIRE(load_modification_file_reader(reader, loaded, meta, true).has_value());

	for (size_t i = 0; i < custom_ids.size(); i++) {
		const std::string& id = custom_ids[i];
		CHECK_MESSAGE(loaded.data("code", id) == "AOcr", id);
		CHECK(loaded.data("dataa1", id) == std::to_string(i));
	}

	CHECK(collect_save_modification_file_errors(loaded, meta, true).empty());
	CHECK_NOTHROW((void)build_modification_file_buffer(loaded, meta, true, false));
}

TEST_CASE("saving a field without a meta data id throws instead of closing the editor") {
	const slk::SLK meta = use_specific_ability_meta();

	// A custom ability whose row has no code, as the old copy_row() left it: "dataa1" then has no meta data id.
	slk::SLK data;
	data.add_row("Z00A");
	data.set_shadow_data("oldid", "Z00A", "AOcr");
	data.set_shadow_data("dataa1", "Z00A", "5");

	const auto errors = collect_save_modification_file_errors(data, meta, true);
	REQUIRE(errors.size() == 1);
	CHECK(errors.front().object_id == "Z00A");
	CHECK(errors.front().field == "dataa1");

	CHECK_THROWS_AS((void)build_modification_file_buffer(data, meta, true, false), std::runtime_error);
}
