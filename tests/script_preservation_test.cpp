// The rules that decide when a save keeps a map's script instead of generating one (src/base/map/script_preservation.ixx). The save
// path itself needs a Map and so OpenGL: it is covered by resave_smoke and `HiveWE --qa-offscreen --qa-kept-script-check`.
// doctest's own forward declaration of std::char_traits breaks std::string here otherwise.
#define DOCTEST_CONFIG_USE_STD_HEADERS
#include <doctest/doctest.h>

import std;
import types;
import Hierarchy;
import MapInfo;
import ScriptPreservation;

namespace fs = std::filesystem;

namespace {
	MapFiles bytes_of(const std::vector<std::pair<std::string, std::string>>& files) {
		MapFiles result;
		for (const auto& file : files) {
			result[fs::path(file.first)].assign(file.second.begin(), file.second.end());
		}
		return result;
	}

	bool names_are(const std::vector<std::string>& changed, const std::vector<std::string>& expected) {
		return changed == expected;
	}

	MapInfo sample_info() {
		MapInfo info;
		PlayerData player {};
		player.internal_number = 0;
		player.type = PlayerType::human;
		player.race = PlayerRace::human;
		player.fixed_start_position = 1;
		player.name = "TRIGSTR_001";
		player.starting_position = { 128.f, -256.f };
		player.ally_low_priorities_flags = 0;
		player.ally_high_priorities_flags = 0;
		info.players.push_back(player);
		ForceData force {};
		force.player_masks = 1;
		force.name = "TRIGSTR_002";
		info.forces.push_back(force);
		return info;
	}
}

// Pinning test: the whole rule. Changing a row here changes which maps lose or keep their script on save.
TEST_CASE("keeps_existing_script keeps a script whose trigger data cannot rebuild it, and only then") {
	struct Row {
		bool script, wtg, wct, keeps;
	};
	const std::array<Row, 8> rows = { {
		{ true, false, false, true },  // script-only or protected map: no wtg, no wct
		{ true, false, true, true },   // wct left behind without its wtg
		{ true, true, false, true },   // wct missing: the map header code (even of a GUI-only map) and custom text would be lost
		{ true, true, true, false },   // normal map: generated as before
		{ false, false, false, false }, // no trigger data and no script: generated as before
		{ false, true, false, false }, // no script to keep
		{ false, true, true, false },
		{ false, false, true, false },
	} };
	for (const Row& row : rows) {
		CAPTURE(row.script);
		CAPTURE(row.wtg);
		CAPTURE(row.wct);
		CHECK(keeps_existing_script({ row.script, row.wtg, row.wct }) == row.keeps);
	}
}

TEST_CASE("the script file names cover the root and the scripts folder for JASS and Lua") {
	const std::set<std::string_view> names(map_script_file_names.begin(), map_script_file_names.end());
	CHECK((names == std::set<std::string_view> { "war3map.j", "war3map.lua", "scripts\\war3map.j", "scripts\\war3map.lua" }));
	for (const std::string_view name : subfolder_script_file_names) {
		CAPTURE(name);
		CHECK(names.contains(name));
		CHECK(name.starts_with("scripts\\"));
	}
	CHECK(subfolder_script_file_names.size() == 2);
}

TEST_CASE("script_inputs takes placed units, regions and the player setup, and nothing else") {
	const MapFiles writes = bytes_of({
		{ "war3mapUnits.doo", "units" },
		{ "war3map.w3r", "regions" },
		{ "war3map.w3e", "terrain" },
		{ "war3map.doo", "doodads" },
	});
	const MapFiles inputs = script_inputs(writes, sample_info());
	CHECK(inputs.size() == 3);
	CHECK(inputs.contains(fs::path("war3mapUnits.doo")));
	CHECK(inputs.contains(fs::path("war3map.w3r")));
	CHECK(inputs.contains(fs::path("players")));
	CHECK_FALSE(inputs.contains(fs::path("war3map.w3e")));
}

TEST_CASE("changed_script_inputs names what the kept script misses") {
	const MapInfo info = sample_info();
	const MapFiles before = script_inputs(bytes_of({ { "war3mapUnits.doo", "units" }, { "war3map.w3r", "regions" } }), info);

	SUBCASE("nothing changed") {
		CHECK(changed_script_inputs(before, script_inputs(bytes_of({ { "war3mapUnits.doo", "units" }, { "war3map.w3r", "regions" } }), info))
				  .empty());
	}
	SUBCASE("a moved unit") {
		const auto changed =
			changed_script_inputs(before, script_inputs(bytes_of({ { "war3mapUnits.doo", "units2" }, { "war3map.w3r", "regions" } }), info));
		CHECK(names_are(changed, { "placed units and items" }));
	}
	SUBCASE("a region removed from the writes") {
		const auto changed = changed_script_inputs(before, script_inputs(bytes_of({ { "war3mapUnits.doo", "units" } }), info));
		CHECK(names_are(changed, { "regions" }));
	}
	SUBCASE("a player, a force or the script language") {
		const MapFiles writes = bytes_of({ { "war3mapUnits.doo", "units" }, { "war3map.w3r", "regions" } });
		MapInfo player_changed = info;
		player_changed.players[0].race = PlayerRace::orc;
		CHECK(names_are(changed_script_inputs(before, script_inputs(writes, player_changed)), { "players, forces or the script language" }));

		MapInfo force_changed = info;
		force_changed.forces[0].allied = true;
		CHECK(changed_script_inputs(before, script_inputs(writes, force_changed)).size() == 1);

		MapInfo lua_changed = info;
		lua_changed.lua = !info.lua;
		CHECK(changed_script_inputs(before, script_inputs(writes, lua_changed)).size() == 1);

		MapInfo author_changed = info;
		author_changed.author = "someone else";
		CHECK(changed_script_inputs(before, script_inputs(writes, author_changed)).empty());
	}
}
