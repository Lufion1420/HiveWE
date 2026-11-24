#include <doctest/doctest.h>

import std;
import SLK;
import MapInfo;

namespace {
// A Weather.slk shaped file: the first column (effectID) is the row key, "name" is a data column.
slk::SLK load_weather_fixture(const std::string& suffix) {
	const auto path = std::filesystem::temp_directory_path() / ("hivewe_weather_fixture_" + suffix + ".slk");
	{
		std::ofstream out(path, std::ios::binary);
		out << "ID;PWXL;N;E\n"
			<< "B;X3;Y4;D0\n"
			<< "C;X1;Y1;K\"effectID\"\n"
			<< "C;X2;Y1;K\"name\"\n"
			<< "C;X1;Y2;K\"RAlr\"\n"
			<< "C;X2;Y2;K\"Ashenvale Rain (Light)\"\n"
			<< "C;X1;Y3;K\"RAhr\"\n"
			<< "C;X2;Y3;K\"Ashenvale Rain (Heavy)\"\n"
			<< "C;X1;Y4;K\"LRaa\"\n"
			<< "C;X2;Y4;K\"Lordaeron Rain\"\n"
			<< "E\n";
	}
	slk::SLK weather(path, true);
	std::filesystem::remove(path);
	return weather;
}

// An EnvironmentSounds.slk shaped file (header verified against the game's file): EnvironmentType is the first column.
slk::SLK load_sound_fixture(const std::string& suffix) {
	const auto path = std::filesystem::temp_directory_path() / ("hivewe_sound_fixture_" + suffix + ".slk");
	{
		std::ofstream out(path, std::ios::binary);
		out << "ID;PWXL;N;E\n"
			<< "B;X3;Y4;D0\n"
			<< "C;X1;Y1;K\"EnvironmentType\"\n"
			<< "C;X2;Y1;K\"DisplayText\"\n"
			<< "C;X1;Y2;K\"Default\"\n"
			<< "C;X2;K\"Default Env\"\n"
			<< "C;X1;Y3;K\"lake\"\n"
			<< "C;X2;K\"Lake\"\n"
			<< "C;X1;Y4;K\"Dungeon\"\n"
			<< "C;X2;K\"Dungeon Env\"\n"
			<< "E\n";
	}
	slk::SLK sounds(path, true);
	std::filesystem::remove(path);
	return sounds;
}
} // namespace

TEST_CASE("global weather id round-trips between w3i bytes and the effect id string") {
	const int id = w3i_weather_id_from_string("RAlr");
	CHECK(w3i_weather_id_to_string(id) == "RAlr");
	// byte-identical to the raw four bytes in the file
	CHECK(std::memcmp(&id, "RAlr", 4) == 0);
	CHECK(w3i_weather_id_to_string(0).empty());
}

TEST_CASE("global weather id conversion is safe for short and long strings") {
	CHECK(w3i_weather_id_from_string("") == 0);
	const int short_id = w3i_weather_id_from_string("AB");
	CHECK(w3i_weather_id_to_string(short_id) == "AB");
	CHECK(w3i_weather_id_from_string("RAlrXYZ") == w3i_weather_id_from_string("RAlr"));
}

TEST_CASE("weather entries come from the SLK row keys and include the first data row") {
	const auto entries = w3i_weather_entries(load_weather_fixture("entries"));
	REQUIRE(entries.size() == 3);
	CHECK(entries[0].name == "Ashenvale Rain (Light)");
	CHECK(entries[0].id == w3i_weather_id_from_string("RAlr"));
	for (const auto& entry : entries) {
		CHECK(entry.id != 0);
		CHECK(!entry.name.empty());
	}
	const auto found = std::ranges::find(entries, w3i_weather_id_from_string("RAlr"), &WeatherEntry::id);
	REQUIRE(found != entries.end());
	CHECK(found->name == "Ashenvale Rain (Light)");
}

TEST_CASE("selecting weather entry k and saving writes entry k's 4-char id") {
	const auto entries = w3i_weather_entries(load_weather_fixture("save"));
	const int stored = w3i_weather_id_from_string("RAlr");
	const char* expected[] = { "RAlr", "RAhr", "LRaa" };
	REQUIRE(entries.size() == 3);
	for (size_t k = 0; k < entries.size(); k++) {
		const int written = w3i_weather_id_for_save(true, entries[k].id, stored);
		CHECK(w3i_weather_id_to_string(written) == expected[k]);
	}
}

TEST_CASE("Map Info weather save: unchanged keeps the id, unchecked clears, nothing selected keeps the stored id") {
	const int stored = w3i_weather_id_from_string("RAlr");
	CHECK(w3i_weather_id_for_save(true, stored, stored) == stored);
	CHECK(w3i_weather_id_for_save(false, stored, stored) == 0);
	CHECK(w3i_weather_id_for_save(true, 0, stored) == stored);
}

TEST_CASE("a stored weather id that is not valid UTF-8 is written back byte-identical") {
	const char raw[4] = { '\x80', 'A', 'l', 'r' };
	int stored = 0;
	std::memcpy(&stored, raw, 4);
	CHECK(w3i_weather_id_from_string(w3i_weather_id_to_string(stored)) == stored);
	CHECK(w3i_weather_id_for_save(true, stored, stored) == stored);
	const int written = w3i_weather_id_for_save(true, stored, 0);
	CHECK(std::memcmp(&written, raw, 4) == 0);
}

TEST_CASE("sound environment entries come from the SLK row keys and include the first data row") {
	const auto entries = w3i_sound_environment_entries(load_sound_fixture("entries"));
	REQUIRE(entries.size() == 3);
	CHECK(entries[0].id == "Default");
	CHECK(entries[0].name == "Default Env");
	CHECK(entries[1].id == "lake");
	CHECK(entries[2].id == "Dungeon");
	for (const auto& entry : entries) {
		CHECK(!entry.id.empty());
		CHECK(!entry.name.empty());
	}
}

TEST_CASE("Map Info sound environment save: selected id is written, unchecked clears, nothing selected keeps the stored id") {
	CHECK(w3i_sound_environment_for_save(true, "lake", "Dungeon") == "lake");
	CHECK(w3i_sound_environment_for_save(true, "Dungeon", "Dungeon") == "Dungeon");
	CHECK(w3i_sound_environment_for_save(false, "lake", "Dungeon").empty());
	CHECK(w3i_sound_environment_for_save(true, "", "Dungeon") == "Dungeon");
}
