#include <doctest/doctest.h>

import std;
import <glm/glm.hpp>;
import Globals;
import MapInfo;
import Terrain;
import Triggers;
import Units;

// World Editor rules verified on all 466 stock 3.0.1 maps. The masks below are synthetic, shaped like the named stock maps.

namespace {
	PlayerData make_player(const int number) {
		PlayerData player {};
		player.internal_number = number;
		player.type = PlayerType::human;
		player.race = PlayerRace::human;
		return player;
	}

	bool contains(const std::string& text, const std::string_view needle) {
		return text.find(needle) != std::string::npos;
	}

	const Terrain& test_terrain() {
		static const Terrain* terrain = new Terrain(); // Never destroyed: its destructor calls GL.
		return *terrain;
	}

	size_t position(const std::string& text, const std::string_view needle) {
		const size_t found = text.find(needle);
		REQUIRE_MESSAGE(found != std::string::npos, needle);
		return found;
	}
} // namespace

TEST_CASE("ally priorities: no priority bits at all writes no InitAllyPriorities") {
	MapInfo info {};
	info.players = { make_player(0), make_player(1) };

	CHECK_FALSE(has_ally_priorities(info));
	CHECK(generate_ally_priorities_script(info, ScriptMode::jass).empty());
	CHECK(generate_ally_priorities_script(info, ScriptMode::lua).empty());

	// A bit for a player number that is not in the list does not count.
	info.players[0].ally_low_priorities_flags = 1u << 7;
	info.players[1].enemy_high_priorities_flags = 1u << 9;
	CHECK_FALSE(has_ally_priorities(info));
	CHECK(generate_ally_priorities_script(info, ScriptMode::lua).empty());
}

TEST_CASE("ally priorities: only the player's own bit writes a count of 1 and no priority (0050, 0148)") {
	MapInfo info {};
	info.players = { make_player(0), make_player(1) };
	info.players[1].ally_low_priorities_flags = 1u << 1;

	CHECK(has_ally_priorities(info));
	const std::string jass = generate_ally_priorities_script(info, ScriptMode::jass);
	CHECK(contains(jass, "call SetStartLocPrioCount(1, 1)\n"));
	CHECK_FALSE(contains(jass, "SetStartLocPrio("));
	CHECK_FALSE(contains(jass, "SetStartLocPrioCount(0,"));
	CHECK_FALSE(contains(jass, "SetEnemyStartLocPrio"));

	const std::string lua = generate_ally_priorities_script(info, ScriptMode::lua);
	CHECK(contains(lua, "function InitAllyPriorities()"));
	CHECK(contains(lua, "SetStartLocPrioCount(1, 1)\n"));
	CHECK_FALSE(contains(lua, "call "));
}

TEST_CASE("ally priorities: enemy masks only write only the enemy calls (0239)") {
	MapInfo info {};
	info.players = { make_player(0), make_player(1) };
	info.players[0].enemy_low_priorities_flags = 1u << 1;
	info.players[1].enemy_high_priorities_flags = 1u << 0;

	CHECK(has_ally_priorities(info));
	for (const ScriptMode mode : { ScriptMode::jass, ScriptMode::lua }) {
		const std::string prefix = mode == ScriptMode::jass ? "call " : "";
		const std::string script = generate_ally_priorities_script(info, mode);
		CHECK(contains(script, prefix + "SetEnemyStartLocPrioCount(0, 1)\n"));
		CHECK(contains(script, prefix + "SetEnemyStartLocPrio(0, 0, 1, MAP_LOC_PRIO_LOW)\n"));
		CHECK(contains(script, prefix + "SetEnemyStartLocPrioCount(1, 1)\n"));
		CHECK(contains(script, prefix + "SetEnemyStartLocPrio(1, 0, 0, MAP_LOC_PRIO_HIGH)\n"));
		CHECK_FALSE(contains(script, "SetStartLocPrio"));
	}
}

TEST_CASE("ally priorities: both bits give LOW, the count includes the own bit (0240)") {
	MapInfo info {};
	info.players = { make_player(0), make_player(1), make_player(2) };
	info.players[0].ally_low_priorities_flags = (1u << 0) | (1u << 1) | (1u << 2);
	info.players[0].ally_high_priorities_flags = (1u << 2);
	info.players[1].ally_high_priorities_flags = (1u << 0);

	const std::string jass = generate_ally_priorities_script(info, ScriptMode::jass);
	CHECK(contains(jass, "call SetStartLocPrioCount(0, 3)\n"));
	CHECK(contains(jass, "call SetStartLocPrio(0, 0, 1, MAP_LOC_PRIO_LOW)\n"));
	CHECK(contains(jass, "call SetStartLocPrio(0, 1, 2, MAP_LOC_PRIO_LOW)\n"));
	CHECK_FALSE(contains(jass, "call SetStartLocPrio(0, 2,"));
	CHECK(contains(jass, "call SetStartLocPrioCount(1, 1)\n"));
	CHECK(contains(jass, "call SetStartLocPrio(1, 0, 0, MAP_LOC_PRIO_HIGH)\n"));
	CHECK_FALSE(contains(jass, "SetStartLocPrioCount(2,"));
}

TEST_CASE("ally priorities: arguments are list positions, not player numbers, ally block before enemy block") {
	MapInfo info {};
	info.players = { make_player(3), make_player(5) };
	info.players[0].ally_low_priorities_flags = 1u << 5;
	info.players[0].enemy_high_priorities_flags = 1u << 5;
	info.players[1].ally_low_priorities_flags = 1u << 3;

	const std::string lua = generate_ally_priorities_script(info, ScriptMode::lua);
	const size_t ally_count_0 = position(lua, "SetStartLocPrioCount(0, 1)\n");
	const size_t ally_prio_0 = position(lua, "SetStartLocPrio(0, 0, 1, MAP_LOC_PRIO_LOW)\n");
	const size_t enemy_count_0 = position(lua, "SetEnemyStartLocPrioCount(0, 1)\n");
	const size_t enemy_prio_0 = position(lua, "SetEnemyStartLocPrio(0, 0, 1, MAP_LOC_PRIO_HIGH)\n");
	const size_t ally_count_1 = position(lua, "SetStartLocPrioCount(1, 1)\n");
	const size_t ally_prio_1 = position(lua, "SetStartLocPrio(1, 0, 0, MAP_LOC_PRIO_LOW)\n");
	CHECK(ally_count_0 < ally_prio_0);
	CHECK(ally_prio_0 < enemy_count_0);
	CHECK(enemy_count_0 < enemy_prio_0);
	CHECK(enemy_prio_0 < ally_count_1);
	CHECK(ally_count_1 < ally_prio_1);
	CHECK_FALSE(contains(lua, "(3"));
	CHECK_FALSE(contains(lua, "(5"));
}

TEST_CASE("water base color: written as R, G, B, A from the stored B, G, R, A bytes when tinting is on (0407)") {
	MapInfo info {};
	info.water_tinting = true;
	info.water_color = glm::u8vec4(0xFF, 0x80, 0x80, 0xFF);
	CHECK(generate_water_base_color_script(info, ScriptMode::jass) == "call SetWaterBaseColor(128, 128, 255, 255)\n");
	CHECK(generate_water_base_color_script(info, ScriptMode::lua) == "SetWaterBaseColor(128, 128, 255, 255)\n");

	info.water_color = glm::u8vec4(10, 20, 30, 40);
	CHECK(generate_water_base_color_script(info, ScriptMode::lua) == "SetWaterBaseColor(30, 20, 10, 40)\n");
}

TEST_CASE("water base color: nothing when the w3i water tinting flag is clear") {
	MapInfo info {};
	info.water_tinting = false;
	info.water_color = glm::u8vec4(0xFF, 0x80, 0x80, 0xFF);
	CHECK(generate_water_base_color_script(info, ScriptMode::jass).empty());
	CHECK(generate_water_base_color_script(info, ScriptMode::lua).empty());
}

TEST_CASE("player mask bit: numbers outside 0 to 31 have no bit") {
	CHECK(player_mask_bit(0) == 1u);
	CHECK(player_mask_bit(31) == 0x80000000u);
	CHECK(player_mask_bit(32) == 0u);
	CHECK(player_mask_bit(-1) == 0u);
}

TEST_CASE("ally priorities: players numbered -1 or 32 match no mask bit") {
	MapInfo info {};
	info.players = { make_player(-1), make_player(32) };
	info.players[0].ally_low_priorities_flags = 0xFFFFFFFFu;
	info.players[1].enemy_high_priorities_flags = 0xFFFFFFFFu;
	CHECK_FALSE(has_ally_priorities(info));
	CHECK(generate_ally_priorities_script(info, ScriptMode::lua).empty());

	// A valid player next to them still gets its priority, and the invalid ones are never targets.
	info.players.push_back(make_player(1));
	const std::string lua = generate_ally_priorities_script(info, ScriptMode::lua);
	CHECK(contains(lua, "SetStartLocPrioCount(0, 1)\n"));
	CHECK(contains(lua, "SetEnemyStartLocPrioCount(1, 1)\n"));
	CHECK(contains(lua, "SetStartLocPrio(0, 0, 2, MAP_LOC_PRIO_LOW)\n"));
	CHECK(contains(lua, "SetEnemyStartLocPrio(1, 0, 2, MAP_LOC_PRIO_HIGH)\n"));
	CHECK_FALSE(contains(lua, "SetStartLocPrioCount(2,"));
}

TEST_CASE("main(): SetWaterBaseColor comes after SetDayNightModels and before NewSoundEnvironment") {
	MapInfo info {};
	info.water_tinting = true;
	info.water_color = glm::u8vec4(0xFF, 0x80, 0x80, 0xFF);

	// main() reads these WorldEditData sections; ctest runs each case in its own process, so the global only holds these.
	for (const std::string_view section : { "TerrainLights", "SoundEnvironment", "DayAmbience", "NightAmbience" }) {
		world_edit_data.set_whole_data(section, "unused", "");
	}

	for (const ScriptMode mode : { ScriptMode::jass, ScriptMode::lua }) {
		const std::string main = generate_main_script(test_terrain(), info, mode);
		const size_t day_night = position(main, "SetDayNightModels(");
		const size_t water = position(main, "SetWaterBaseColor(128, 128, 255, 255)\n");
		const size_t sound = position(main, "NewSoundEnvironment(");
		CHECK(day_night < water);
		CHECK(water < sound);
	}

	info.water_tinting = false;
	CHECK_FALSE(contains(generate_main_script(test_terrain(), info, ScriptMode::lua), "SetWaterBaseColor"));
}

TEST_CASE("config(): InitAllyPriorities is called only when some player has priorities") {
	MapInfo info {};
	info.players = { make_player(0), make_player(1) };
	const Units units;

	for (const ScriptMode mode : { ScriptMode::jass, ScriptMode::lua }) {
		const std::string config = generate_map_configuration_script(test_terrain(), units, info, mode);
		CHECK(contains(config, "InitCustomPlayerSlots"));
		CHECK_FALSE(contains(config, "InitAllyPriorities"));
	}

	info.players[0].ally_low_priorities_flags = 1u << 1;
	for (const ScriptMode mode : { ScriptMode::jass, ScriptMode::lua }) {
		const std::string config = generate_map_configuration_script(test_terrain(), units, info, mode);
		CHECK(contains(config, mode == ScriptMode::jass ? "call InitAllyPriorities()\n" : "InitAllyPriorities()\n"));
	}
}

TEST_CASE("w3i colours: stored B, G, R, A convert to R, G, B, A and back unchanged") {
	// Stock 0415: fog bytes 131, 37, 111, 255 are written by the World Editor as SetTerrainFogEx(..., 0.435, 0.145, 0.514).
	const glm::u8vec4 stored(131, 37, 111, 255);
	const glm::u8vec4 rgba = w3i_color_to_rgba(stored);
	CHECK(rgba == glm::u8vec4(111, 37, 131, 255));
	CHECK(rgba_to_w3i_color(rgba) == stored);

	// The alpha byte is kept as it is, so an unchanged dialog writes the same bytes back.
	const glm::u8vec4 translucent(1, 2, 3, 77);
	CHECK(w3i_color_to_rgba(translucent) == glm::u8vec4(3, 2, 1, 77));
	CHECK(rgba_to_w3i_color(w3i_color_to_rgba(translucent)) == translucent);
}
