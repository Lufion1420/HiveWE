#include <doctest/doctest.h>

import std;
import MapInfo;
import Triggers;

namespace {
	PlayerData make_player(const int number, const PlayerType type, const PlayerRace race, const int start_flags) {
		PlayerData player {};
		player.internal_number = number;
		player.type = type;
		player.race = race;
		player.fixed_start_position = start_flags;
		return player;
	}

	bool contains(const std::string& text, const std::string_view needle) {
		return text.find(needle) != std::string::npos;
	}
} // namespace

// Rules verified on all 466 stock 3.0.1 maps (second differential sweep, kinds 3, 4 and 5).

TEST_CASE("two human players, w3i flags 0: teams together, every slot race selectable") {
	MapInfo info {};
	info.fixed_player_settings = false;
	info.custom_forces = false;
	info.players = {
		make_player(0, PlayerType::human, PlayerRace::human, 0),
		make_player(1, PlayerType::human, PlayerRace::orc, 0),
	};
	info.forces.resize(1);

	CHECK(
		generate_map_settings_script(info, ScriptMode::jass)
		== "call SetPlayers(2)\ncall SetTeams(2)\ncall SetGamePlacement(MAP_PLACEMENT_TEAMS_TOGETHER)\n"
	);
	CHECK(
		generate_map_settings_script(info, ScriptMode::lua)
		== "SetPlayers(2)\nSetTeams(2)\nSetGamePlacement(MAP_PLACEMENT_TEAMS_TOGETHER)\n"
	);

	const std::string slots = generate_player_slots_script(info, ScriptMode::jass);
	CHECK(contains(slots, "call SetPlayerRaceSelectable(Player(0), true)\n"));
	CHECK(contains(slots, "call SetPlayerRaceSelectable(Player(1), true)\n"));
	CHECK_FALSE(contains(slots, "ForcePlayerStartLocation"));
}

TEST_CASE("one force, one human of two players, w3i flags 0x60: map settings, race fixed") {
	MapInfo info {};
	info.fixed_player_settings = true; // 0x20
	info.custom_forces = true; // 0x40
	info.players = {
		make_player(0, PlayerType::human, PlayerRace::human, 0),
		make_player(1, PlayerType::computer, PlayerRace::undead, 0),
	};
	info.forces.resize(1);

	CHECK(
		generate_map_settings_script(info, ScriptMode::lua)
		== "SetPlayers(2)\nSetTeams(2)\nSetGamePlacement(MAP_PLACEMENT_USE_MAP_SETTINGS)\n"
	);
	CHECK(
		generate_map_settings_script(info, ScriptMode::jass)
		== "call SetPlayers(2)\ncall SetTeams(2)\ncall SetGamePlacement(MAP_PLACEMENT_USE_MAP_SETTINGS)\n"
	);

	const std::string slots = generate_player_slots_script(info, ScriptMode::lua);
	CHECK(contains(slots, "SetPlayerRaceSelectable(Player(0), false)\n"));
	CHECK(contains(slots, "SetPlayerRaceSelectable(Player(1), false)\n"));
	CHECK_FALSE(contains(slots, "true"));
}

TEST_CASE("w3i flags 0x60: race-selectable slot (race 0, dword 0x2) stays selectable and is not forced") {
	MapInfo info {};
	info.fixed_player_settings = true;
	info.custom_forces = true;
	info.players = {
		make_player(0, PlayerType::human, PlayerRace::selectable, 0x2),
		make_player(1, PlayerType::computer, PlayerRace::orc, 0x1),
		make_player(2, PlayerType::human, PlayerRace::human, 0x2),
	};
	info.forces.resize(1);

	const std::string slots = generate_player_slots_script(info, ScriptMode::jass);
	// Dword bit 0x2 alone makes a slot with a fixed race preference selectable.
	CHECK(contains(slots, "call SetPlayerRaceSelectable(Player(2), true)\n"));
	CHECK(contains(slots, "call SetPlayerRaceSelectable(Player(0), true)\n"));
	CHECK_FALSE(contains(slots, "ForcePlayerStartLocation(Player(0)"));
	// Bit 0x1 alone forces the start location and does not make the race selectable.
	CHECK(contains(slots, "call ForcePlayerStartLocation(Player(1), 1)\n"));
	CHECK(contains(slots, "call SetPlayerRaceSelectable(Player(1), false)\n"));
}

TEST_CASE("w3i flag 0x20: race 0 with dword 0 is selectable and not forced") {
	MapInfo info {};
	info.fixed_player_settings = true;
	info.players = {make_player(0, PlayerType::human, PlayerRace::selectable, 0)};

	const std::string slots = generate_player_slots_script(info, ScriptMode::jass);
	CHECK(contains(slots, "call SetPlayerRaceSelectable(Player(0), true)\n"));
	CHECK_FALSE(contains(slots, "ForcePlayerStartLocation"));
}

TEST_CASE("w3i flag 0x20: dword 0x3 forces the start location and keeps the race selectable") {
	MapInfo info {};
	info.fixed_player_settings = true;
	info.players = {make_player(0, PlayerType::human, PlayerRace::night_elf, 0x3)};

	const std::string slots = generate_player_slots_script(info, ScriptMode::jass);
	CHECK(contains(slots, "call ForcePlayerStartLocation(Player(0), 0)\n"));
	CHECK(contains(slots, "call SetPlayerRaceSelectable(Player(0), true)\n"));
}

TEST_CASE("no players: SetTeams(0) and map settings placement") {
	MapInfo info {};
	info.forces.resize(1);

	CHECK(
		generate_map_settings_script(info, ScriptMode::jass)
		== "call SetPlayers(0)\ncall SetTeams(0)\ncall SetGamePlacement(MAP_PLACEMENT_USE_MAP_SETTINGS)\n"
	);
}

TEST_CASE("only human slots count for placement: computer, neutral and rescuable do not") {
	MapInfo info {};
	info.players = {
		make_player(0, PlayerType::human, PlayerRace::human, 0),
		make_player(1, PlayerType::computer, PlayerRace::orc, 0),
		make_player(2, PlayerType::neutral, PlayerRace::undead, 0),
		make_player(3, PlayerType::rescuable, PlayerRace::night_elf, 0),
	};

	CHECK(
		generate_map_settings_script(info, ScriptMode::lua)
		== "SetPlayers(4)\nSetTeams(4)\nSetGamePlacement(MAP_PLACEMENT_USE_MAP_SETTINGS)\n"
	);

	info.players[1].type = PlayerType::human;
	CHECK(
		generate_map_settings_script(info, ScriptMode::lua)
		== "SetPlayers(4)\nSetTeams(4)\nSetGamePlacement(MAP_PLACEMENT_TEAMS_TOGETHER)\n"
	);
}
