module;

#include <nlohmann/json.hpp>

export module ScenarioDataIo;

import std;
import MapInfo;
import TriggerStrings;

export inline constexpr const char* scenario_data_format = "hivewe-scenario-v1";

export struct ScenarioPlayerImport {
	int internal_number = -1;
	std::string name;
	PlayerType controller = PlayerType::human;
	PlayerRace race = PlayerRace::selectable;
	bool fixed_start_location = false;
};

export struct ScenarioForceImport {
	std::string name;
	std::vector<int> players;
	bool allied = false;
	bool allied_victory = false;
	bool share_vision = false;
	bool share_unit_control = false;
	bool share_advanced_unit_control = false;
};

export struct ScenarioImportData {
	bool custom_forces = false;
	bool fixed_player_settings = false;
	bool modif_ally_priorities = false;
	std::vector<ScenarioPlayerImport> players;
	std::vector<ScenarioForceImport> forces;
};

namespace {

std::string controller_to_string(const PlayerType type) {
	switch (type) {
		case PlayerType::human:
			return "human";
		case PlayerType::computer:
			return "computer";
		case PlayerType::neutral:
			return "neutral";
		case PlayerType::rescuable:
			return "rescuable";
	}
	return "human";
}

std::optional<PlayerType> controller_from_string(const std::string& value) {
	if (value == "human") {
		return PlayerType::human;
	}
	if (value == "computer") {
		return PlayerType::computer;
	}
	if (value == "neutral") {
		return PlayerType::neutral;
	}
	if (value == "rescuable") {
		return PlayerType::rescuable;
	}
	return std::nullopt;
}

std::string race_to_string(const PlayerRace race) {
	switch (race) {
		case PlayerRace::selectable:
			return "selectable";
		case PlayerRace::human:
			return "human";
		case PlayerRace::orc:
			return "orc";
		case PlayerRace::undead:
			return "undead";
		case PlayerRace::night_elf:
			return "night_elf";
	}
	return "selectable";
}

std::optional<PlayerRace> race_from_string(const std::string& value) {
	if (value == "selectable") {
		return PlayerRace::selectable;
	}
	if (value == "human") {
		return PlayerRace::human;
	}
	if (value == "orc") {
		return PlayerRace::orc;
	}
	if (value == "undead") {
		return PlayerRace::undead;
	}
	if (value == "night_elf") {
		return PlayerRace::night_elf;
	}
	return std::nullopt;
}

} // namespace

/// Serializes the current map's players/forces into a human-editable JSON document,
/// suitable for hand-editing in a text editor and re-importing via apply_scenario_import.
export std::string build_scenario_json(const MapInfo& info, TriggerStrings& trigger_strings) {
	nlohmann::json root;
	root["format"] = scenario_data_format;
	root["custom_forces"] = info.custom_forces;
	root["fixed_player_settings"] = info.fixed_player_settings;
	root["modif_ally_priorities"] = info.modif_ally_priorities;

	nlohmann::json players = nlohmann::json::array();
	for (const auto& player : info.players) {
		nlohmann::json entry;
		entry["internal_number"] = player.internal_number;
		entry["name"] = std::string(trigger_strings.string(player.name));
		entry["controller"] = controller_to_string(player.type);
		entry["race"] = race_to_string(player.race);
		entry["fixed_start_location"] = player.fixed_start_position != 0;
		players.push_back(std::move(entry));
	}
	root["players"] = players;

	nlohmann::json forces = nlohmann::json::array();
	for (const auto& force : info.forces) {
		nlohmann::json entry;
		entry["name"] = std::string(trigger_strings.string(force.name));

		nlohmann::json members = nlohmann::json::array();
		for (const auto& player : info.players) {
			if (force.player_masks & (1 << player.internal_number)) {
				members.push_back(player.internal_number);
			}
		}
		entry["players"] = members;

		entry["allied"] = force.allied;
		entry["allied_victory"] = force.allied_victory;
		entry["share_vision"] = force.share_vision;
		entry["share_unit_control"] = force.share_unit_control;
		entry["share_advanced_unit_control"] = force.share_advanced_unit_control;
		forces.push_back(std::move(entry));
	}
	root["forces"] = forces;

	return root.dump(2);
}

/// Parses a scenario.json document (as produced by build_scenario_json, or hand-edited).
export std::expected<ScenarioImportData, std::string> parse_scenario_json(const std::string& json_text) {
	nlohmann::json root;
	try {
		root = nlohmann::json::parse(json_text);
	} catch (const std::exception& ex) {
		return std::unexpected(std::format("Failed to parse scenario.json: {}", ex.what()));
	}

	ScenarioImportData data;
	data.custom_forces = root.value("custom_forces", false);
	data.fixed_player_settings = root.value("fixed_player_settings", false);
	data.modif_ally_priorities = root.value("modif_ally_priorities", false);

	if (root.contains("players")) {
		for (const auto& entry : root["players"]) {
			ScenarioPlayerImport player;
			player.internal_number = entry.value("internal_number", -1);
			if (player.internal_number < 0) {
				return std::unexpected("scenario.json: player entry is missing a valid 'internal_number'.");
			}
			player.name = entry.value("name", "");

			const std::string controller_text = entry.value("controller", "human");
			const auto controller = controller_from_string(controller_text);
			if (!controller) {
				return std::unexpected(
					std::format("scenario.json: unknown controller '{}' for player {}.", controller_text, player.internal_number)
				);
			}
			player.controller = *controller;

			const std::string race_text = entry.value("race", "selectable");
			const auto race = race_from_string(race_text);
			if (!race) {
				return std::unexpected(std::format("scenario.json: unknown race '{}' for player {}.", race_text, player.internal_number));
			}
			player.race = *race;

			player.fixed_start_location = entry.value("fixed_start_location", false);
			data.players.push_back(std::move(player));
		}
	}

	if (root.contains("forces")) {
		for (const auto& entry : root["forces"]) {
			ScenarioForceImport force;
			force.name = entry.value("name", "");
			if (entry.contains("players")) {
				for (const auto& player_number : entry["players"]) {
					force.players.push_back(player_number.get<int>());
				}
			}
			force.allied = entry.value("allied", false);
			force.allied_victory = entry.value("allied_victory", false);
			force.share_vision = entry.value("share_vision", false);
			force.share_unit_control = entry.value("share_unit_control", false);
			force.share_advanced_unit_control = entry.value("share_advanced_unit_control", false);
			data.forces.push_back(std::move(force));
		}
	}

	if (data.players.empty() || data.forces.empty()) {
		return std::unexpected("scenario.json: no players or forces were found.");
	}

	return data;
}

/// Applies parsed scenario data onto a live MapInfo, matching players by internal_number
/// and wholesale-replacing forces. Ally/enemy priority flags are left untouched (out of scope).
export void apply_scenario_import(const ScenarioImportData& data, MapInfo& info, TriggerStrings& trigger_strings) {
	info.custom_forces = data.custom_forces;
	info.fixed_player_settings = data.fixed_player_settings;
	info.modif_ally_priorities = data.modif_ally_priorities;

	for (const auto& imported_player : data.players) {
		const auto found = std::ranges::find_if(info.players, [&](const PlayerData& player) {
			return player.internal_number == imported_player.internal_number;
		});
		if (found == info.players.end()) {
			continue;
		}
		trigger_strings.set_string(found->name, imported_player.name);
		found->type = imported_player.controller;
		found->race = imported_player.race;
		found->fixed_start_position = imported_player.fixed_start_location ? 1 : 0;
	}

	std::vector<ForceData> new_forces;
	new_forces.reserve(data.forces.size());
	for (const auto& imported_force : data.forces) {
		ForceData force;
		trigger_strings.set_string(force.name, imported_force.name);
		force.allied = imported_force.allied;
		force.allied_victory = imported_force.allied_victory;
		force.share_vision = imported_force.share_vision;
		force.share_unit_control = imported_force.share_unit_control;
		force.share_advanced_unit_control = imported_force.share_advanced_unit_control;
		force.player_masks = 0;
		for (const int player_internal_number : imported_force.players) {
			force.player_masks |= (1 << player_internal_number);
		}
		new_forces.push_back(std::move(force));
	}
	info.forces = std::move(new_forces);
}
