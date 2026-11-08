export module MapInfo;

import std;
import types;
import BinaryReader;
import BinaryWriter;
import Hierarchy;
import MapFormatCheck;
import Utilities;
import <glm/glm.hpp>;

export enum class PlayerType {
	human,
	computer,
	neutral,
	rescuable
};

export enum class PlayerRace {
	selectable,
	human,
	orc,
	undead,
	night_elf
};

export struct PlayerData {
	int internal_number;
	PlayerType type;
	PlayerRace race;
	uint32_t hud_skin = 64; // Only in the file for v39 from editor 6140 on.
	int fixed_start_position;
	std::string name;
	glm::vec2 starting_position;
	uint32_t ally_low_priorities_flags;
	uint32_t ally_high_priorities_flags;
	uint32_t enemy_low_priorities_flags = 0;
	uint32_t enemy_high_priorities_flags = 0;
};

export struct ForceData {
	bool allied;
	bool allied_victory;
	bool share_vision;
	bool share_unit_control;
	bool share_advanced_unit_control;
	int player_masks;
	std::string name;
	uint32_t unknown_flags = 0; // Bits without a field above, written back as they were read.
};

struct UpgradeAvailability {
	int player_flags;
	std::string id;
	int level;
	int availability;
};

struct TechAvailability {
	int player_flags;
	std::string id;
};

struct RandomUnitLine {
	int chance;
	std::vector<std::string> ids;
};

struct RandomUnitTable {
	int creation_number;
	std::string name;
	std::vector<int> positions;
	std::vector<RandomUnitLine> lines;
};

struct RandomItemTable {
	int creation_number;
	std::string name;
	std::vector<ItemSet> item_sets;
};

export class MapInfo {
  public:
	/// The version the file was read as. A v39 map saves as v39 with its own editor and game stamps;
	/// anything older saves as write_version.
	int format_version = write_version;
	int map_version;
	int editor_version;
	int game_version_major;
	int game_version_minor;
	int game_version_patch;
	int game_version_build;
	std::string name;
	std::string author;
	std::string description;
	std::string suggested_players;

	glm::vec2 camera_left_bottom;
	glm::vec2 camera_right_top;
	glm::vec2 camera_left_top;
	glm::vec2 camera_right_bottom;

	glm::ivec4 camera_complements;

	int playable_width;
	int playable_height;

	int all_flags = 0;
	bool hide_minimap_preview;
	bool modif_ally_priorities;
	bool melee_map;
	bool unknown;
	bool masked_area_partially_visible;
	bool fixed_player_settings;
	bool custom_forces;
	bool custom_techtree;
	bool custom_abilities;
	bool custom_upgrades;
	bool unknown2;
	bool cliff_shore_waves;
	bool rolling_shore_waves;
	bool unknown3;
	bool unknown4;
	bool item_classification;
	bool water_tinting;
	bool accurate_probability_for_calculations;
	bool custom_ability_skins;
	bool disable_deny_icon;
	bool force_default_zoom;
	bool force_max_zoom;
	bool force_min_zoom;

	int loading_screen_number;
	uint32_t race_hud = 0; // v39
	std::string loading_screen_model;
	std::string loading_screen_text;
	std::string loading_screen_title;
	std::string loading_screen_subtitle;

	int game_data_set;

	std::string prologue_screen_model;
	std::string prologue_text;
	std::string prologue_title;
	std::string prologue_subtitle;

	int fog_style = 0;
	float fog_start_z_height = 0.f;
	float fog_end_z_height = 0.f;
	float fog_density = 0.f;
	glm::u8vec4 fog_color = glm::u8vec4(0);

	// v39
	float fog_height_start = 0.f;
	float fog_height_end = 0.f;
	float fog_linear_start = 0.f;
	float fog_linear_end = 0.f;
	float fog_max_opacity = 0.f;
	int fog_draw_over_sky = 0;

	int weather_id = 0;
	std::string custom_sound_environment;
	char custom_light_tileset;
	glm::u8vec4 water_color = glm::u8vec4(0);

	bool lua = false;
	uint32_t supported_modes = 0;
	uint32_t game_data_version = 0;

	uint32_t default_cam_distance = 0;
	uint32_t max_cam_distance = 0;
	uint32_t min_cam_distance = 0;

	// v39
	int hd_water_min_opacity = 0;
	int hd_water_max_opacity = 0;
	int hd_water_reflectivity = 0;
	int hd_water_emissivity = 0;
	int hd_water_edge_softness = 0;
	int hd_water_waves_vertex_displacement = 0;
	int hd_water_waves_normal_map_strength = 0;
	glm::u8vec4 hd_water_override_color = glm::u8vec4(0);
	int hd_water_envmap_reflectivity = 0;
	glm::u8vec4 alpha_tile_minimap_color = glm::u8vec4(0);

	std::vector<PlayerData> players;
	std::vector<ForceData> forces;
	std::vector<UpgradeAvailability> available_upgrades;
	std::vector<TechAvailability> available_tech;
	std::vector<RandomUnitTable> random_unit_tables;
	std::vector<RandomItemTable> random_item_tables;

	static constexpr int write_version = 33;
	static constexpr int write_editor_version = 6116;
	static constexpr int write_game_version_major = 2;
	static constexpr int write_game_version_minor = 0;
	static constexpr int write_game_version_patch = 3;
	static constexpr int write_game_version_build = 22978;

	/// Editors 6128-6139 are refused by map_format::require_known_w3i_editor, so this never guesses for them.
	bool has_player_hud_skin() const {
		return format_version == 39 && static_cast<uint32_t>(editor_version) >= 6140;
	}

	void load() {
		BinaryReader reader = hierarchy.map_file_read("war3map.w3i").value();

		const int version = reader.read<uint32_t>();

		map_format::require_accepted("war3map.w3i", map_format::w3i_versions, version);
		format_version = version;
		const bool v39 = version == 39;

		// Only v39 is held to this. Older files keep the leniency they always had.
		const auto read_count = [&](const uint64_t smallest_record_size) -> uint32_t {
			const uint32_t count = reader.read<uint32_t>();
			if (v39 && count * smallest_record_size > static_cast<uint64_t>(reader.remaining())) {
				throw std::runtime_error(std::format("war3map.w3i is damaged: a count of {} does not fit in the file.", count));
			}
			return count;
		};

		if (version >= 18) {
			map_version = reader.read<uint32_t>();
			editor_version = reader.read<uint32_t>();
			map_format::require_known_w3i_editor(version, editor_version);

			if (version >= 28) {
				game_version_major = reader.read<uint32_t>();
				game_version_minor = reader.read<uint32_t>();
				game_version_patch = reader.read<uint32_t>();
				game_version_build = reader.read<uint32_t>();
			}
		}
		name = reader.read_c_string();
		author = reader.read_c_string();
		description = reader.read_c_string();
		suggested_players = reader.read_c_string();

		camera_left_bottom = reader.read<glm::vec2>();
		camera_right_top = reader.read<glm::vec2>();
		camera_left_top = reader.read<glm::vec2>();
		camera_right_bottom = reader.read<glm::vec2>();

		camera_complements = reader.read<glm::ivec4>();

		playable_width = reader.read<uint32_t>();
		playable_height = reader.read<uint32_t>();

		const int flags = reader.read<uint32_t>();
		all_flags = flags;
		hide_minimap_preview = flags & 0x0001;
		modif_ally_priorities = flags & 0x0002;
		melee_map = flags & 0x0004;
		unknown = flags & 0x0008; // playable map size was large
		masked_area_partially_visible = flags & 0x0010;
		fixed_player_settings = flags & 0x0020;
		custom_forces = flags & 0x0040;
		custom_techtree = flags & 0x0080;
		custom_abilities = flags & 0x0100;
		custom_upgrades = flags & 0x0200;
		unknown2 = flags & 0x0400; // has properties menu been opened
		cliff_shore_waves = flags & 0x0800;
		rolling_shore_waves = flags & 0x1000;
		unknown3 = flags & 0x2000; // has terrain fog
		unknown4 = flags & 0x4000; // requires expansion
		item_classification = flags & 0x8000;
		water_tinting = flags & 0x10000;
		accurate_probability_for_calculations = flags & 0x20000;
		custom_ability_skins = flags & 0x40000;
		disable_deny_icon = flags & 0x80000;
		force_default_zoom = flags & 0x100000;
		force_max_zoom = flags & 0x200000;
		force_min_zoom = flags & 0x400000;

		// Tileset
		reader.advance(1);

		if (version >= 25) { // TFT
			loading_screen_number = reader.read<uint32_t>();
			if (v39) {
				race_hud = reader.read<uint32_t>();
			}
			loading_screen_model = reader.read_c_string();
			loading_screen_text = reader.read_c_string();
			loading_screen_title = reader.read_c_string();
			loading_screen_subtitle = reader.read_c_string();

			game_data_set = reader.read<uint32_t>();

			prologue_screen_model = reader.read_c_string();
			prologue_text = reader.read_c_string();
			prologue_title = reader.read_c_string();
			prologue_subtitle = reader.read_c_string();

			fog_style = reader.read<uint32_t>();
			fog_start_z_height = reader.read<float>();
			fog_end_z_height = reader.read<float>();
			fog_density = reader.read<float>();
			fog_color = reader.read<glm::u8vec4>();

			if (v39) {
				fog_height_start = reader.read<float>();
				fog_height_end = reader.read<float>();
				fog_linear_start = reader.read<float>();
				fog_linear_end = reader.read<float>();
				fog_max_opacity = reader.read<float>();
				fog_draw_over_sky = reader.read<int32_t>();
			}

			weather_id = reader.read<uint32_t>();
			custom_sound_environment = reader.read_c_string();
			custom_light_tileset = reader.read<uint8_t>();
			water_color = reader.read<glm::u8vec4>();

			if (version >= 28) {
				lua = reader.read<uint32_t>() == 1;
			}

			if (version >= 31) {
				supported_modes = reader.read<uint32_t>();
				game_data_version = reader.read<uint32_t>();
			}
			if (version >= 32) {
				default_cam_distance = reader.read<uint32_t>();
				max_cam_distance = reader.read<uint32_t>();
				if (version >= 33) {
					min_cam_distance = reader.read<uint32_t>();
				}
			}
			if (v39) {
				hd_water_min_opacity = reader.read<int32_t>();
				hd_water_max_opacity = reader.read<int32_t>();
				hd_water_reflectivity = reader.read<int32_t>();
				hd_water_emissivity = reader.read<int32_t>();
				hd_water_edge_softness = reader.read<int32_t>();
				hd_water_waves_vertex_displacement = reader.read<int32_t>();
				hd_water_waves_normal_map_strength = reader.read<int32_t>();
				hd_water_override_color = reader.read<glm::u8vec4>();
				hd_water_envmap_reflectivity = reader.read<int32_t>();
				alpha_tile_minimap_color = reader.read<glm::u8vec4>();
			}
		} else if (version == 18) { // RoC
			loading_screen_number = reader.read<uint32_t>();
			loading_screen_text = reader.read_c_string();
			loading_screen_title = reader.read_c_string();
			loading_screen_subtitle = reader.read_c_string();

			// game_data_set = reader.read<uint32_t>();
			reader.advance(4); // ToDo RoC map loading screen number

			prologue_text = reader.read_c_string();
			prologue_title = reader.read_c_string();
			prologue_subtitle = reader.read_c_string();
		} else {
			reader.advance(1); // unknown, loading screen number but only 1 digit?
			loading_screen_text = reader.read_c_string();
			loading_screen_title = reader.read_c_string();
			loading_screen_subtitle = reader.read_c_string();
			reader.advance(4); // prologue stuff?
		}

		players.resize(read_count(has_player_hud_skin() ? 45 : 41));
		for (auto&& i : players) {
			i.internal_number = reader.read<uint32_t>();
			i.type = static_cast<PlayerType>(reader.read<uint32_t>() - 1);
			i.race = static_cast<PlayerRace>(reader.read<uint32_t>());
			if (has_player_hud_skin()) {
				i.hud_skin = reader.read<uint32_t>();
			}
			i.fixed_start_position = reader.read<uint32_t>();
			i.name = reader.read_c_string();
			i.starting_position = reader.read<glm::vec2>();
			i.ally_low_priorities_flags = reader.read<uint32_t>();
			i.ally_high_priorities_flags = reader.read<uint32_t>();
			if (version >= 31) {
				i.enemy_low_priorities_flags = reader.read<uint32_t>();
				i.enemy_high_priorities_flags = reader.read<uint32_t>();
			}
		}

		forces.resize(read_count(9));
		for (auto&& i : forces) {
			const uint32_t force_flags = reader.read<uint32_t>();
			i.allied = force_flags & 0b00000001;
			i.allied_victory = force_flags & 0b00000010;
			i.share_vision = force_flags & 0b00000100;
			i.share_unit_control = force_flags & 0b00010000;
			i.share_advanced_unit_control = force_flags & 0b00100000;
			i.unknown_flags = force_flags & ~0b00110111u;

			i.player_masks = reader.read<uint32_t>();
			i.name = reader.read_c_string();
		}

		if (!v39 && reader.remaining() < 4) {
			return;
		}

		available_upgrades.resize(read_count(16));
		for (auto&& i : available_upgrades) {
			i.player_flags = reader.read<uint32_t>();
			i.id = reader.read_string(4);
			i.level = reader.read<uint32_t>();
			i.availability = reader.read<uint32_t>();
		}

		if (!v39 && reader.remaining() < 4) {
			return;
		}

		available_tech.resize(read_count(8));
		for (auto&& i : available_tech) {
			i.player_flags = reader.read<uint32_t>();
			i.id = reader.read_string(4);
		}

		if (!v39 && reader.remaining() < 4) {
			return;
		}

		random_unit_tables.resize(read_count(13));
		for (auto&& i : random_unit_tables) {
			i.creation_number = reader.read<uint32_t>();
			i.name = reader.read_c_string();
			i.positions = reader.read_vector<int>(reader.read<uint32_t>());

			i.lines.resize(read_count(4 + 4 * i.positions.size()));
			for (auto&& j : i.lines) {
				j.chance = reader.read<uint32_t>();
				for (size_t k = 0; k < i.positions.size(); k++) {
					j.ids.push_back(reader.read_string(4));
				}
			}
		}

		if (!v39 && reader.remaining() < 4) {
			return;
		}

		if (version >= 25) {
			random_item_tables.resize(read_count(9));
			for (auto&& i : random_item_tables) {
				i.creation_number = reader.read<uint32_t>();
				i.name = reader.read_c_string();
				i.item_sets.resize(read_count(4));
				for (auto&& j : i.item_sets) {
					j.items.resize(read_count(8));
					for (auto&& [chance, id] : j.items) {
						chance = reader.read<uint32_t>();
						id = reader.read_string(4);
					}
				}
			}
		}

		if (v39 && reader.remaining() != 0) {
			throw std::runtime_error(std::format("war3map.w3i is damaged: {} bytes follow the last section.", reader.remaining()));
		}
	}

	void save(char tileset) const {
		BinaryWriter writer;

		const bool v39 = format_version == 39;

		writer.write(v39 ? 39 : write_version);
		writer.write(map_version);
		writer.write(v39 ? editor_version : write_editor_version);
		writer.write(v39 ? game_version_major : write_game_version_major);
		writer.write(v39 ? game_version_minor : write_game_version_minor);
		writer.write(v39 ? game_version_patch : write_game_version_patch);
		writer.write(v39 ? game_version_build : write_game_version_build);
		writer.write_c_string(name);
		writer.write_c_string(author);
		writer.write_c_string(description);
		writer.write_c_string(suggested_players);

		writer.write(camera_left_bottom);
		writer.write(camera_right_top);
		writer.write(camera_left_top);
		writer.write(camera_right_bottom);

		writer.write(camera_complements);

		writer.write(playable_width);
		writer.write(playable_height);

		const int known_flags = hide_minimap_preview * 0x0001 | modif_ally_priorities * 0x0002 | melee_map * 0x0004 | unknown * 0x0008
			| masked_area_partially_visible * 0x0010 | fixed_player_settings * 0x0020 | custom_forces * 0x0040 | custom_techtree * 0x0080
			| custom_abilities * 0x0100 | custom_upgrades * 0x0200 | unknown2 * 0x0400 | cliff_shore_waves * 0x0800
			| rolling_shore_waves * 0x1000 | unknown3 * 0x2000 | unknown4 * 0x4000 | item_classification * 0x8000 | water_tinting * 0x10000
			| accurate_probability_for_calculations * 0x20000 | custom_ability_skins * 0x40000 | disable_deny_icon * 0x80000
			| force_default_zoom * 0x100000 | force_max_zoom * 0x200000 | force_min_zoom * 0x400000;

		writer.write((all_flags & ~0x7FFFFF) | known_flags);

		writer.write(tileset);

		writer.write(loading_screen_number);
		if (v39) {
			writer.write(race_hud);
		}
		writer.write_c_string(loading_screen_model);
		writer.write_c_string(loading_screen_text);
		writer.write_c_string(loading_screen_title);
		writer.write_c_string(loading_screen_subtitle);

		writer.write(game_data_set);

		writer.write_c_string(prologue_screen_model);
		writer.write_c_string(prologue_text);
		writer.write_c_string(prologue_title);
		writer.write_c_string(prologue_subtitle);

		writer.write(fog_style);
		writer.write(fog_start_z_height);
		writer.write(fog_end_z_height);
		writer.write(fog_density);
		writer.write(fog_color);

		if (v39) {
			writer.write(fog_height_start);
			writer.write(fog_height_end);
			writer.write(fog_linear_start);
			writer.write(fog_linear_end);
			writer.write(fog_max_opacity);
			writer.write(fog_draw_over_sky);
		}

		writer.write(weather_id);
		writer.write_c_string(custom_sound_environment);
		writer.write(custom_light_tileset);
		writer.write(water_color);

		writer.write((uint32_t)lua);

		writer.write(supported_modes);
		writer.write(game_data_version);

		writer.write(default_cam_distance);
		writer.write(max_cam_distance);
		writer.write(min_cam_distance);

		if (v39) {
			writer.write(hd_water_min_opacity);
			writer.write(hd_water_max_opacity);
			writer.write(hd_water_reflectivity);
			writer.write(hd_water_emissivity);
			writer.write(hd_water_edge_softness);
			writer.write(hd_water_waves_vertex_displacement);
			writer.write(hd_water_waves_normal_map_strength);
			writer.write(hd_water_override_color);
			writer.write(hd_water_envmap_reflectivity);
			writer.write(alpha_tile_minimap_color);
		}

		writer.write<uint32_t>(players.size());
		for (const auto& i : players) {
			writer.write(i.internal_number);
			writer.write(static_cast<int>(i.type) + 1);
			writer.write(static_cast<int>(i.race));
			if (has_player_hud_skin()) {
				writer.write(i.hud_skin);
			}
			writer.write(i.fixed_start_position);
			writer.write_c_string(i.name);
			writer.write(i.starting_position);
			writer.write(i.ally_low_priorities_flags);
			writer.write(i.ally_high_priorities_flags);
			writer.write(i.enemy_low_priorities_flags);
			writer.write(i.enemy_high_priorities_flags);
		}

		writer.write<uint32_t>(forces.size());
		for (const auto& i : forces) {
			const uint32_t force_flags = i.allied * 0b00000001 | i.allied_victory * 0b00000010 | i.share_vision * 0b00000100
				| i.share_unit_control * 0b00010000 | i.share_advanced_unit_control * 0b00100000 | i.unknown_flags;
			writer.write(force_flags);

			writer.write(i.player_masks);
			writer.write_c_string(i.name);
		}

		writer.write<uint32_t>(available_upgrades.size());
		for (const auto& i : available_upgrades) {
			writer.write(i.player_flags);
			writer.write_c_string_padded(i.id, 4);
			writer.write(i.level);
			writer.write(i.availability);
		}

		writer.write<uint32_t>(available_tech.size());
		for (const auto& i : available_tech) {
			writer.write(i.player_flags);
			writer.write_c_string_padded(i.id, 4);
		}

		writer.write<uint32_t>(random_unit_tables.size());
		for (const auto& i : random_unit_tables) {
			writer.write(i.creation_number);
			writer.write_c_string(i.name);
			writer.write<uint32_t>(i.positions.size());
			writer.write_vector(i.positions);

			writer.write<uint32_t>(i.lines.size());
			for (const auto& j : i.lines) {
				writer.write(j.chance);
				for (const auto& id : j.ids) {
					writer.write_c_string_padded(id, 4);
				}
			}
		}

		writer.write<uint32_t>(random_item_tables.size());
		for (const auto& i : random_item_tables) {
			writer.write(i.creation_number);
			writer.write_c_string(i.name);

			writer.write<uint32_t>(i.item_sets.size());
			for (const auto& j : i.item_sets) {
				writer.write<uint32_t>(j.items.size());
				for (const auto& [chance, id] : j.items) {
					writer.write(chance);
					writer.write_c_string_padded(id, 4);
				}
			}
		}

		hierarchy.map_file_write("war3map.w3i", writer.buffer);
	}

	void update_map_bounds_info(
		int unplayable_left,
		int unplayable_right,
		int unplayable_top,
		int unplayable_bottom,
		int terrain_width,
		int terrain_height,
		float terrain_offset_x,
		float terrain_offset_y
	) {
		// update unplayable area in map info
		camera_complements[0] = unplayable_left;
		camera_complements[1] = unplayable_right;
		camera_complements[2] = unplayable_bottom;
		camera_complements[3] = unplayable_top;

		// update playable map area
		playable_width = terrain_width - 1 - camera_complements[0] - camera_complements[1];
		playable_height = terrain_height - 1 - camera_complements[2] - camera_complements[3];

		// compute camera bounds based on complements and terrain offset
		// these bounds are used in the generated JASS script
		camera_left_bottom.x = (camera_complements[0] + 4) * 128.f + terrain_offset_x;
		camera_left_bottom.y = (camera_complements[2] + 2) * 128.f + terrain_offset_y;

		camera_right_top.x = (terrain_width - 1 - camera_complements[1] - 4) * 128.f + terrain_offset_x;
		camera_right_top.y = (terrain_height - 1 - camera_complements[3] - 2) * 128.f + terrain_offset_y;

		camera_left_top.x = (camera_complements[0] + 4) * 128.f + terrain_offset_x;
		camera_left_top.y = (terrain_height - 1 - camera_complements[3] - 2) * 128.f + terrain_offset_y;

		camera_right_bottom.x = (terrain_width - 1 - camera_complements[1] - 4) * 128.f + terrain_offset_x;
		camera_right_bottom.y = (camera_complements[2] + 2) * 128.f + terrain_offset_y;
	}
};
