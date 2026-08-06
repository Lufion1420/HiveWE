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
