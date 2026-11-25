export module ScriptPreservation;

import std;
import types;
import Hierarchy;
import MapInfo;

namespace fs = std::filesystem;

/// Every place a map script can be in a map folder or archive. Map protectors and older tools put it under scripts\ (MPQ names use
/// backslashes). Needs verification: that Warcraft III 3.0 reads scripts\war3map.lua when the root has none. Either way HiveWE only
/// keeps these files as they are and never writes there.
export constexpr std::array<std::string_view, 4> map_script_file_names = {
	"war3map.j",
	"war3map.lua",
	"scripts\\war3map.j",
	"scripts\\war3map.lua",
};

/// The script files outside the archive root. Opening an archive extracts them by name, so an archive without a (listfile) keeps them.
export constexpr std::array<std::string_view, 2> subfolder_script_file_names = {
	"scripts\\war3map.j",
	"scripts\\war3map.lua",
};

/// What a map folder holds of its trigger data, as Map::load sees it.
export struct TriggerDataPresence {
	bool script = false; // any of map_script_file_names
	bool wtg = false;
	bool wct = false;
};

/// True when saving must keep the map's existing script instead of generating one: the trigger data HiveWE would generate it from is
/// missing or incomplete, so a generated script would lose code. That is a script with war3map.wtg or war3map.wct missing: without the
/// wtg there are no triggers (protected or script-only maps); without the wct the map header code and every custom-text trigger's code
/// are gone, and even a GUI-only map keeps its header code there. Without any script there is nothing to lose, and the script is
/// generated as before. This is the one switch for that rule.
export bool keeps_existing_script(const TriggerDataPresence& presence) {
	return presence.script && (!presence.wtg || !presence.wct);
}

/// Shown once when such a map is opened (stderr for --resave).
export constexpr std::string_view kept_script_notice =
	"This map has a script but no complete trigger data (war3map.wtg or war3map.wct is missing), "
	"so HiveWE cannot rebuild the script from triggers. Saving keeps the map script exactly as it is and writes no trigger files. "
	"Trigger editing is turned off for this map.";

/// Shown where trigger editing is refused for such a map.
export constexpr std::string_view trigger_editing_off_text =
	"Trigger editing is off for this map: it has a script but no complete trigger data, so HiveWE keeps its script as it is.";

/// The map data a generated script is made from that a save writes: placed units and items, regions, and players, forces and the
/// script language. Cameras and sounds are not written by Map::save (their files stay as loaded), so they cannot drift from a kept
/// script. Keys are file names, or "players" for the w3i part.
export MapFiles script_inputs(const MapFiles& captured_writes, const MapInfo& info) {
	MapFiles inputs;
	for (const std::string_view name : { "war3mapUnits.doo", "war3map.w3r" }) {
		if (const auto found = captured_writes.find(fs::path(name)); found != captured_writes.end()) {
			inputs[fs::path(name)] = found->second;
		}
	}

	// Start positions are left out: save() copies them from the start location units, which war3mapUnits.doo already covers, and
	// comparing the w3i copy would warn on a save that only made it match those units.
	std::string players = std::format("lua={}\n", info.lua);
	for (const PlayerData& player : info.players) {
		players += std::format("player {} {} {} {} {} {} {} {} {}\n", player.internal_number, static_cast<int>(player.type),
							   static_cast<int>(player.race), player.fixed_start_position, player.name, player.ally_low_priorities_flags,
							   player.ally_high_priorities_flags, player.enemy_low_priorities_flags, player.enemy_high_priorities_flags);
	}
	for (const ForceData& force : info.forces) {
		players += std::format("force {} {} {} {} {} {} {} {}\n", force.allied, force.allied_victory, force.share_vision,
							   force.share_unit_control, force.share_advanced_unit_control, force.player_masks, force.name, force.unknown_flags);
	}
	inputs[fs::path("players")].assign(players.begin(), players.end());
	return inputs;
}

/// What differs between two script_inputs results, as words for the save warning. Empty when nothing does.
export std::vector<std::string> changed_script_inputs(const MapFiles& before, const MapFiles& after) {
	const auto label = [](const fs::path& key) -> std::string {
		if (key == "war3mapUnits.doo") {
			return "placed units and items";
		}
		if (key == "war3map.w3r") {
			return "regions";
		}
		if (key == "players") {
			return "players, forces or the script language";
		}
		const std::u8string text = key.u8string();
		return std::string(text.begin(), text.end());
	};

	std::vector<std::string> changed;
	for (const auto& [key, bytes] : after) {
		const auto found = before.find(key);
		if (found == before.end() || found->second != bytes) {
			changed.push_back(label(key));
		}
	}
	for (const auto& [key, bytes] : before) {
		if (!after.contains(key)) {
			changed.push_back(label(key));
		}
	}
	return changed;
}
