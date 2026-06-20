#include <doctest/doctest.h>

import std;
import types;
import no_init_allocator;
import BinaryReader;
import BinaryWriter;
import SLK;
import ModificationTables;
import ObjectDataText;
import ObjectDataIo;
import TriggerStrings;

using namespace std::string_literals;
namespace fs = std::filesystem;

TEST_CASE("build_modification_file_buffer round-trip") {
	slk::SLK meta;
	meta.add_row("Ocr6");
	meta.set_shadow_data("field", "Ocr6", "data");
	meta.set_shadow_data("repeat", "Ocr6", "4");
	meta.set_shadow_data("data", "Ocr6", "1");
	meta.set_shadow_data("type", "Ocr6", "int");
	meta.build_meta_map();

	slk::SLK data;
	data.add_row("hfoo");
	data.base_data["hfoo"] = {};
	data.copy_row("hfoo", "Test", false);
	data.set_shadow_data("dataa1", "Test", "67");

	const std::vector<u8> buffer = build_modification_file_buffer(data, meta, true, false);
	BinaryReader reader(std::vector<u8, default_init_allocator<u8>>(buffer.begin(), buffer.end()));

	const auto extracted = extract_modification_shadow_map(std::move(reader), data, meta, true);
	REQUIRE(extracted);
	CHECK(extracted->contains("Test"));
	CHECK(extracted->at("Test").at("oldid") == "hfoo");
	CHECK(extracted->at("Test").at("dataa1") == "67");
}

TEST_CASE("validate_shadow_map rejects invalid parent") {
	slk::SLK meta;
	meta.add_row("unam");
	meta.set_shadow_data("field", "unam", "name");
	meta.set_shadow_data("displayname", "unam", "Name");
	meta.set_shadow_data("type", "unam", "string");
	meta.build_meta_map();

	slk::SLK data;
	data.add_row("hpea");

	ModificationShadowMap shadow_map;
	shadow_map["Abcd"]["oldid"] = "zzzz";
	shadow_map["Abcd"]["name"] = "Bad Unit";

	ObjectCategoryDescriptor descriptor{
		ObjectDataCategory::unit,
		"unit",
		"Unit",
		"war3map.w3u",
		"war3mapSkin.w3u",
		"unit.ini",
		&data,
		&meta,
		nullptr,
		false,
	};

	const ImportValidationReport report = validate_shadow_map(descriptor, shadow_map, true);
	CHECK(report.has_blocking_errors());
}

TEST_CASE("text table round-trip") {
	slk::SLK meta;
	meta.add_row("unam");
	meta.set_shadow_data("field", "unam", "name");
	meta.set_shadow_data("displayname", "unam", "Name");
	meta.set_shadow_data("type", "unam", "string");
	meta.build_meta_map();

	slk::SLK data;
	data.add_row("hpea");
	data.set_shadow_data("name", "hpea", "Edited Footman");

	const std::string text = build_object_text_table(data, meta, { .w3x2lni_format = false });
	REQUIRE(!text.empty());
	CHECK(text.find("; Name (string)") != std::string::npos);

	const fs::path temp_path = fs::temp_directory_path() / "hivewe_object_data_test_unit.ini";
	REQUIRE(write_text_file(temp_path, text));

	const ParsedTextTable parsed = parse_object_text_table(temp_path, false);
	REQUIRE(parsed.issues.empty());
	REQUIRE(parsed.objects.size() == 1);
	CHECK(parsed.objects.front().id == "hpea");
	CHECK(parsed.objects.front().fields.at("name") == "Edited Footman");

	fs::remove(temp_path);
}

TEST_CASE("text export resolves TRIGSTR references") {
	slk::SLK meta;
	meta.add_row("unam");
	meta.set_shadow_data("field", "unam", "name");
	meta.set_shadow_data("displayname", "unam", "Name");
	meta.set_shadow_data("type", "unam", "string");
	meta.build_meta_map();

	slk::SLK data;
	data.add_row("hpea");
	data.set_shadow_data("name", "hpea", "TRIGSTR_001");

	TriggerStrings trigger_strings;
	std::string trigstr_key = "TRIGSTR_001";
	trigger_strings.set_string(trigstr_key, "Peasant");

	const std::string text = build_object_text_table(data, meta, { .trigger_strings = &trigger_strings });
	CHECK(text.find("Peasant") != std::string::npos);
	CHECK(text.find("TRIGSTR_001") == std::string::npos);
	CHECK(text.find("; Name (string)") != std::string::npos);
}

TEST_CASE("merge_import_category respects overwrite selection") {
	ModificationShadowMap current;
	current["Abcd"]["name"] = "Existing";

	ModificationShadowMap imported;
	imported["Abcd"]["name"] = "Imported";
	imported["Efgt"]["name"] = "New Object";

	const auto merged_skip = merge_import_category(current, imported, {});
	CHECK(merged_skip.at("Abcd").at("name") == "Existing");
	CHECK(merged_skip.at("Efgt").at("name") == "New Object");

	const auto merged_overwrite = merge_import_category(current, imported, { "Abcd" });
	CHECK(merged_overwrite.at("Abcd").at("name") == "Imported");
}

TEST_CASE("detect_import_conflicts ignores unchanged objects and new objects") {
	slk::SLK meta;
	meta.add_row("unam");
	meta.set_shadow_data("field", "unam", "name");
	meta.set_shadow_data("displayname", "unam", "Name");
	meta.set_shadow_data("type", "unam", "string");
	meta.build_meta_map();

	slk::SLK data;
	data.add_row("hpea");
	data.set_shadow_data("name", "hpea", "Footman");
	data.set_shadow_data("name", "Abcd", "Custom Unit");
	data.set_shadow_data("oldid", "Abcd", "hpea");

	ObjectCategoryDescriptor descriptor{
		ObjectDataCategory::unit,
		"unit",
		"Unit",
		"war3map.w3u",
		"war3mapSkin.w3u",
		"unit.ini",
		&data,
		&meta,
		nullptr,
		false,
	};

	ModificationShadowMap imported;
	imported["Abcd"]["oldid"] = "hpea";
	imported["Abcd"]["name"] = "Custom Unit";
	imported["Efgt"]["oldid"] = "hpea";
	imported["Efgt"]["name"] = "Brand New";

	const auto conflicts = detect_import_conflicts(descriptor, imported);
	CHECK(conflicts.empty());
}

TEST_CASE("detect_import_conflicts reports objects with changed fields") {
	slk::SLK meta;
	meta.add_row("unam");
	meta.set_shadow_data("field", "unam", "name");
	meta.set_shadow_data("displayname", "unam", "Name");
	meta.set_shadow_data("type", "unam", "string");
	meta.build_meta_map();

	slk::SLK data;
	data.add_row("hpea");
	data.set_shadow_data("name", "hpea", "Footman");
	data.set_shadow_data("name", "Abcd", "Custom Unit");
	data.set_shadow_data("oldid", "Abcd", "hpea");

	ObjectCategoryDescriptor descriptor{
		ObjectDataCategory::unit,
		"unit",
		"Unit",
		"war3map.w3u",
		"war3mapSkin.w3u",
		"unit.ini",
		&data,
		&meta,
		nullptr,
		false,
	};

	ModificationShadowMap imported;
	imported["Abcd"]["oldid"] = "hpea";
	imported["Abcd"]["name"] = "Changed Unit";

	const auto conflicts = detect_import_conflicts(descriptor, imported);
	REQUIRE(conflicts.size() == 1);
	CHECK(conflicts.front().id == "Abcd");
	CHECK(conflicts.front().category == ObjectDataCategory::unit);
}

TEST_CASE("import_value_matches_current resolves TRIGSTR like text export") {
	TriggerStrings trigger_strings;
	std::string trigstr_key = "TRIGSTR_001";
	trigger_strings.set_string(trigstr_key, "Resolved Name");
	CHECK(import_value_matches_current("Resolved Name", "TRIGSTR_001", "string", &trigger_strings));
	CHECK_FALSE(import_value_matches_current("Different Name", "TRIGSTR_001", "string", &trigger_strings));
}

TEST_CASE("collect_save_modification_file_errors catches unknown field") {
	slk::SLK meta;
	meta.add_row("unam");
	meta.set_shadow_data("field", "unam", "name");
	meta.set_shadow_data("displayname", "unam", "Name");
	meta.set_shadow_data("type", "unam", "string");
	meta.build_meta_map();

	slk::SLK data;
	data.add_row("hpea");
	data.set_shadow_data("notafield", "hpea", "value");

	const auto errors = collect_save_modification_file_errors(data, meta, false);
	CHECK(!errors.empty());
}

TEST_CASE("text export comment round-trip preserves quoted semicolons") {
	slk::SLK meta;
	meta.add_row("unam");
	meta.set_shadow_data("field", "unam", "name");
	meta.set_shadow_data("displayname", "unam", "Name");
	meta.set_shadow_data("type", "unam", "string");
	meta.build_meta_map();

	slk::SLK data;
	data.add_row("hpea");
	data.set_shadow_data("name", "hpea", "Unit; Hero");

	const std::string text = build_object_text_table(data, meta, { .w3x2lni_format = false });
	CHECK(text.find("; Name (string)") != std::string::npos);

	const ParsedTextTable parsed = parse_object_text_content(text, false);
	REQUIRE(parsed.issues.empty());
	REQUIRE(parsed.objects.size() == 1);
	CHECK(parsed.objects.front().fields.at("name") == "Unit; Hero");
}

TEST_CASE("text parser joins multiline quoted values") {
	const std::string sample =
		"[A000]\n"
		"oldid = \"ANcl\"\n"
		"ubertip1 = \"|cffff9619Description|r|nLong tooltip text continues here\n"
		"\" ; Tooltip - Normal - Extended (string)\n";

	const ParsedTextTable parsed = parse_object_text_content(sample, false);
	REQUIRE(parsed.issues.empty());
	REQUIRE(parsed.objects.size() == 1);
	CHECK(parsed.objects.front().fields.at("ubertip1").contains("Long tooltip text continues here"));
}

TEST_CASE("text parser strips export comments after escaped trailing quotes") {
	const std::string sample =
		"[A000]\n"
		"oldid = \"ANcl\"\n"
		R"(ubertip1 = "|cffff9619Description:|r |nTobi cannot attack while the spell is active\" ; Tooltip - Normal - Extended (string))"
		"\n"
		"ubertip2 = \"Second tooltip\"\n";

	const ParsedTextTable parsed = parse_object_text_content(sample, false);
	REQUIRE(parsed.issues.empty());
	REQUIRE(parsed.objects.size() == 1);
	CHECK(parsed.objects.front().fields.at("ubertip1").ends_with("active\""));
	CHECK(parsed.objects.front().fields.at("ubertip1").find("Tooltip - Normal") == std::string::npos);
	CHECK(parsed.objects.front().fields.at("ubertip1").find("ubertip2") == std::string::npos);
	CHECK(parsed.objects.front().fields.contains("ubertip2"));
	CHECK(parsed.objects.front().fields.at("ubertip2") == "Second tooltip");
}

TEST_CASE("text export round-trip preserves trailing quote in string values") {
	slk::SLK meta;
	meta.add_row("aub1");
	meta.set_shadow_data("field", "aub1", "ubertip1");
	meta.set_shadow_data("displayname", "aub1", "Tooltip - Normal - Extended");
	meta.set_shadow_data("type", "aub1", "string");
	meta.build_meta_map();

	slk::SLK data;
	data.add_row("ANcl");
	data.set_shadow_data("ubertip1", "A000", "Ends with a quote: test\"");
	data.set_shadow_data("oldid", "A000", "ANcl");

	const std::string text = build_object_text_table(data, meta, { .w3x2lni_format = false });
	CHECK(text.find("; Tooltip - Normal - Extended (string)") != std::string::npos);
	CHECK(text.find("Ends with a quote: test\\\"") != std::string::npos);

	const ParsedTextTable parsed = parse_object_text_content(text, false);
	REQUIRE(parsed.issues.empty());
	REQUIRE(parsed.objects.size() == 1);
	CHECK(parsed.objects.front().fields.at("ubertip1") == "Ends with a quote: test\"");
}

TEST_CASE("text parser handles legacy export lines missing closing quote before comment") {
	const std::string sample =
		"[A000]\n"
		"oldid = \"ANcl\"\n"
		R"(ubertip1 = "|cffff9619Description:|r |nTobi cannot attack while the spell is active\" ; Tooltip - Normal - Extended (string))"
		"\n"
		"ubertip2 = \"Second tooltip\" ; Tooltip - Normal - Extended (string)\n";

	const ParsedTextTable parsed = parse_object_text_content(sample, false);
	REQUIRE(parsed.issues.empty());
	REQUIRE(parsed.objects.size() == 1);
	CHECK(parsed.objects.front().fields.at("ubertip1").ends_with("active\""));
	CHECK(parsed.objects.front().fields.at("ubertip1").find("Tooltip - Normal") == std::string::npos);
	CHECK(parsed.objects.front().fields.at("ubertip2") == "Second tooltip");
}

TEST_CASE("parse dummyObjectEditorData text export") {
	const fs::path root = fs::path(__FILE__).parent_path().parent_path() / "dummyObjectEditorData" / "table";
	if (!fs::exists(root)) {
		return;
	}

	for (const fs::directory_entry& entry : fs::directory_iterator(root)) {
		if (entry.path().extension() != ".ini") {
			continue;
		}

		const ParsedTextTable parsed = parse_object_text_table(entry.path(), false);
		for (const TextParseIssue& issue : parsed.issues) {
			MESSAGE(entry.path().filename().string() << " line " << issue.line << ": " << issue.message);
		}
		CHECK_MESSAGE(parsed.issues.empty(), entry.path().filename().string().c_str());
	}
}

TEST_CASE("w3x2lni text import maps _parent to oldid") {
	const fs::path temp_path = fs::temp_directory_path() / "hivewe_object_data_test_w3x2lni.ini";
	REQUIRE(write_text_file(temp_path, "[Abcd]\n_parent = \"hpea\"\nname = \"Imported\"\n"));

	const ParsedTextTable parsed = parse_object_text_table(temp_path, true);
	REQUIRE(parsed.issues.empty());
	REQUIRE(parsed.objects.size() == 1);
	CHECK(parsed.objects.front().fields.at("oldid") == "hpea");

	fs::remove(temp_path);
}
