// The two pieces the unsaved changes check (Map::serialized_state) is built on. The check itself needs a Map and so OpenGL: it is
// covered by `HiveWE --qa-offscreen --qa-unsaved-check`.
#include <doctest/doctest.h>

import std;
import types;
import SLK;
import Hierarchy;
import TriggerStrings;

namespace fs = std::filesystem;

namespace {
	slk::SLK sample_table() {
		slk::SLK table;
		table.add_column("name");
		table.add_column("hp");
		table.base_data["hfoo"]["name"] = "Footman";
		table.base_data["hfoo"]["hp"] = "420";
		table.base_data["hpea"]["name"] = "Peasant";
		return table;
	}
}

TEST_CASE("serialized shadow data follows the edits, not the order they were made in") {
	slk::SLK first = sample_table();
	const std::string unedited = first.serialized_shadow_data();

	first.set_shadow_data("name", "hfoo", "Guard");
	first.set_shadow_data("hp", "hfoo", "500");
	first.set_shadow_data("name", "hpea", "Worker");
	CHECK(first.serialized_shadow_data() != unedited);

	slk::SLK second = sample_table();
	second.set_shadow_data("name", "hpea", "Worker");
	second.set_shadow_data("hp", "hfoo", "500");
	second.set_shadow_data("name", "hfoo", "Guard");
	CHECK(second.serialized_shadow_data() == first.serialized_shadow_data());

	second.set_shadow_data("hp", "hfoo", "501");
	CHECK(second.serialized_shadow_data() != first.serialized_shadow_data());

	// Back to the stock values: nothing is left to save.
	first.set_shadow_data("name", "hfoo", "Footman");
	first.set_shadow_data("hp", "hfoo", "420");
	first.set_shadow_data("name", "hpea", "Peasant");
	CHECK(first.serialized_shadow_data() == unedited);
}

TEST_CASE("serialized shadow data keeps entries apart and counts a row without fields") {
	slk::SLK first = sample_table();
	slk::SLK second = sample_table();
	first.shadow_data["hfoo"]["name"] = "a2:hpx";
	second.shadow_data["hfoo"]["name"] = "a";
	second.shadow_data["hfoo"]["hp"] = "x";
	CHECK(first.serialized_shadow_data() != second.serialized_shadow_data());

	slk::SLK empty_row = sample_table();
	empty_row.shadow_data["hfoo"] = {};
	CHECK(empty_row.serialized_shadow_data() != sample_table().serialized_shadow_data());
}

TEST_CASE("MapWriteCapture collects map writes and touches no file") {
	const fs::path folder = fs::temp_directory_path() / "hivewe_map_write_capture_test";
	fs::remove_all(folder);
	fs::create_directories(folder);
	const fs::path previous_directory = hierarchy.map_directory;
	hierarchy.map_directory = folder;

	TriggerStrings strings;
	std::string key;
	strings.set_string(key, "captured text");

	MapFiles files;
	{
		const MapWriteCapture capture(files);
		strings.save();
	}
	CHECK_FALSE(fs::exists(folder / "war3map.wts"));
	REQUIRE(files.contains("war3map.wts"));
	const std::vector<u8>& bytes = files.at("war3map.wts");
	CHECK(std::string(bytes.begin(), bytes.end()).find("captured text") != std::string::npos);

	// After the capture, the same writer writes the file again, with the captured bytes.
	strings.save();
	std::ifstream stream(folder / "war3map.wts", std::ios::binary);
	const std::string written((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	stream.close();
	CHECK(written == std::string(bytes.begin(), bytes.end()));

	// A writer that throws does not leave the capture installed.
	try {
		const MapWriteCapture capture(files);
		throw std::runtime_error("writer failed");
	} catch (const std::runtime_error&) {
	}
	CHECK(Hierarchy::map_write_capture == nullptr);

	hierarchy.map_directory = previous_directory;
	fs::remove_all(folder);
}
