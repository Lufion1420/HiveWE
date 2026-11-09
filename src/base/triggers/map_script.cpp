module;

#include <QDir>
#include <QProcess>

module Triggers;

import std;
import INI;
namespace fs = std::filesystem;

void generate_item_globals(MapScriptWriter& script, const std::unordered_map<std::string, std::string>& item_variables) {
	for (const auto& [creation_number, type] : item_variables) {
		script.global("item", "gg_item_" + type + "_" + creation_number, script.null());
	}
}

void generate_global_variables(
	MapScriptWriter& script,
	std::unordered_map<std::string, std::string>& unit_variables,
	const std::unordered_map<std::string, std::string>& item_variables,
	std::unordered_map<std::string, std::string>& destructable_variables,
	const std::vector<Trigger>& triggers,
	const std::vector<TriggerVariable>& variables,
	const ini::INI& trigger_data,
	const Regions& regions,
	const GameCameras& cameras,
	const Sounds& sounds
) {
	if (script.mode == ScriptMode::jass) {
		script.write_ln("globals");
	}

	if (script.mode == ScriptMode::jass) {
		for (const auto& variable : variables) {
			const std::string base_type = trigger_data.data("TriggerTypes", variable.type, 4);
			const std::string type = base_type.empty() ? variable.type : base_type;
			if (variable.is_array) {
				script.write_ln(std::format("{} array udg_{}", type, variable.name));
			} else {
				std::string default_value = trigger_data.data("TriggerTypeDefaults", type);

				if (default_value.empty()) { // handle?
					default_value = "null";
				}

				script.global(type, "udg_" + variable.name, default_value);
			}
		}
	} else {
		for (const auto& variable : variables) {
			const std::string base_type = trigger_data.data("TriggerTypes", variable.type, 4);
			const std::string type = base_type.empty() ? variable.type : base_type;
			script.global(variable.type, "udg_" + variable.name, lua_global_default(type, variable.is_array));
		}
	}

	for (const auto& i : regions.regions) {
		script.global("rect", prefixed_script_identifier("gg_rct_", i.name), script.null());
	}

	for (const auto& i : cameras.cameras) {
		script.global("camerasetup", prefixed_script_identifier("gg_cam_", i.name), script.null());
	}

	for (const auto& i : sounds.sounds) {
		const std::string sound_name = prefixed_script_identifier("", i.name);

		if (i.music) {
			// Music files are stored as just a file path string
			script.global("string", sound_name, "\"\"");
		} else {
			script.global("sound", sound_name, script.null());
		}
	}

	for (const auto& i : triggers) {
		if (!emits_trigger_identifier(i, script.mode)) {
			continue;
		}

		script.global("trigger", prefixed_script_identifier("gg_trg_", i.name), script.null());
	}

	for (const auto& [creation_number, type] : unit_variables) {
		script.global("unit", "gg_unit_" + type + "_" + creation_number, script.null());
	}

	generate_item_globals(script, item_variables);

	for (const auto& [creation_number, type] : destructable_variables) {
		script.global("destructable", "gg_dest_" + type + "_" + creation_number, script.null());
	}

	if (script.mode == ScriptMode::jass) {
		script.write_ln("endglobals");
	}
}

void generate_init_global_variables(MapScriptWriter& script, const std::vector<TriggerVariable>& variables, const ini::INI& trigger_data) {
	script.function("InitGlobals", [&]() {
		script.local("integer", "i", "0");

		for (const auto& variable : variables) {
			const std::string base_type = trigger_data.data("TriggerTypes", variable.type, 4);
			const std::string type = base_type.empty() ? variable.type : base_type;
			const std::string default_value = trigger_data.data("TriggerTypeDefaults", type);

			if (!variable.is_initialized && default_value.empty()) {
				continue;
			}

			std::string value;
			if (variable.is_initialized) {
				const std::string converted_value = trigger_data.data("TriggerParams", variable.initial_value, 2);

				if (converted_value.empty()) {
					if (type == "string") {
						value = "\"" + variable.initial_value + "\"";
					} else {
						value = variable.initial_value;
					}
				} else {
					value = converted_value;
				}
			} else {
				if (type == "string") {
					value = "\"\"";
				} else {
					value = default_value;
				}
			}

			if (variable.is_array) {
				script.forloop(0, variable.array_size, "i", [&] {
					script.set_variable("udg_" + variable.name + "[i]", value);
				});
			} else {
				script.set_variable("udg_" + variable.name, value);
			}
		}
	});
}

/// Reads an integer units_slk field, or `fallback` when the type has no value for it.
int units_slk_int_or(const std::string_view field, const std::string_view id, const int fallback) {
	const std::string_view value = units_slk.data<std::string_view>(field, id);
	int output = fallback;
	if (value.empty() || std::from_chars(value.data(), value.data() + value.size(), output).ec != std::errc()) {
		return fallback;
	}
	return output;
}

/// Whether the unit type's merged ability list (custom w3u uabi included) contains the gold mine ability Agld.
bool has_gold_mine_ability(const std::string_view id) {
	for (const auto ability : std::views::split(units_slk.data<std::string_view>("abillist", id), ',')) {
		if (std::string_view(ability.begin(), ability.end()) == "Agld") {
			return true;
		}
	}
	return false;
}

void generate_units(
	MapScriptWriter& script,
	const std::unordered_map<std::string, std::string>& unit_variables,
	const Terrain& terrain,
	const Units& units
) {
	script.function("CreateAllUnits", [&]() {
		script.local("unit", "u", script.null());
		script.local("integer", "unitID", "0");
		script.local("trigger", "t", script.null());
		script.local("real", "life", "0");

		for (const auto& i : units.units) {
			if (i.id == "sloc") {
				continue;
			}

			std::string unit_reference = "u";
			const auto unit_variable = unit_variables.find(std::format("{:0>4}", i.creation_number));
			if (unit_variable != unit_variables.end() && unit_variable->second == i.id) {
				unit_reference = std::format("gg_unit_{}_{:0>4}", i.id, i.creation_number);
			}

			script.set_variable(
				unit_reference,
				std::format(
					"BlzCreateUnitWithSkin(Player({}), {}, {:.4f}, {:.4f}, {:.4f}, {})",
					i.player,
					script.four_cc(i.id),
					i.position.x * 128.f + terrain.offset.x,
					i.position.y * 128.f + terrain.offset.y,
					glm::degrees(i.angle),
					script.four_cc(i.skin_id)
				)
			);

			if (i.health != -1) {
				script.set_variable("life", std::format("GetUnitState({}, {})", unit_reference, "UNIT_STATE_LIFE"));
				script.call("SetUnitState", unit_reference, "UNIT_STATE_LIFE", std::to_string(i.health / 100.f) + " * life");
			}

			if (i.mana != -1) {
				script.call("SetUnitState", unit_reference, "UNIT_STATE_MANA", i.mana);
			}

			// The World Editor writes the stored gold for every gold mine, including the 12500 default.
			if (i.id == "ugol" || has_gold_mine_ability(i.id)) {
				script.call("SetResourceAmount", unit_reference, i.gold);
			}

			if (i.level != 1) {
				script.call("SetHeroLevel", unit_reference, i.level, "false");
			}

			if (i.strength != 0) {
				script.call("SetHeroStr", unit_reference, i.strength, "true");
			}

			if (i.agility != 0) {
				script.call("SetHeroAgi", unit_reference, i.agility, "true");
			}

			if (i.intelligence != 0) {
				script.call("SetHeroInt", unit_reference, i.intelligence, "true");
			}

			float range;
			if (i.target_acquisition != -1.f) {
				if (i.target_acquisition == -2.f) {
					range = 200.f;
				} else {
					range = i.target_acquisition;
				}
				script.call("SetUnitAcquireRange", unit_reference, range);
			}

			// A custom colour only counts on types with customteamcolor set; otherwise the type's teamcolor, if any, is written.
			int color = units_slk_int_or("teamcolor", i.id, -1);
			if (units_slk_int_or("customteamcolor", i.id, 0) == 1 && i.custom_color != -1) {
				color = i.custom_color;
			}
			if (color != -1) {
				script.call("SetUnitColor", unit_reference, std::format("ConvertPlayerColor({})", color));
			}

			for (const auto& j : i.abilities) {
				for (size_t k = 0; k < std::get<2>(j); k++) {
					script.call("SelectHeroSkill", unit_reference, script.four_cc(std::get<0>(j)));
				}

				if (std::get<1>(j)) {
					std::string_view order_on = abilities_slk.data<std::string_view>("orderon", std::get<0>(j));
					if (order_on.empty()) {
						order_on = abilities_slk.data<std::string_view>("order", std::get<0>(j));
					}
					script.call("IssueImmediateOrder", unit_reference, std::format("\"{}\"", order_on));

				} else {
					std::string_view order_off = abilities_slk.data<std::string_view>("orderoff", std::get<0>(j));
					if (!order_off.empty()) {
						script.call("IssueImmediateOrder", unit_reference, std::format("\"{}\"", order_off));
					}
				}
			}

			for (const auto& j : i.items) {
				script.call("UnitAddItemToSlotById", unit_reference, script.four_cc(j.second), j.first);
			}

			if (!i.item_sets.empty()) {
				script.set_variable("t", "CreateTrigger()");
				script.call("TriggerRegisterUnitEvent", "t", unit_reference, "EVENT_UNIT_DEATH");
				script.call("TriggerRegisterUnitEvent", "t", unit_reference, "EVENT_UNIT_CHANGE_OWNER");

				if (script.mode == ScriptMode::jass) {
					script.call("TriggerAddAction", "t", "function UnitItemDrops_" + std::to_string(i.creation_number));
				} else {
					script.call("TriggerAddAction", "t", "UnitItemDrops_" + std::to_string(i.creation_number));
				}
			} else if (i.item_table_pointer != -1) {
				script.set_variable("t", "CreateTrigger()");
				script.call("TriggerRegisterUnitEvent", "t", unit_reference, "EVENT_UNIT_DEATH");
				script.call("TriggerRegisterUnitEvent", "t", unit_reference, "EVENT_UNIT_CHANGE_OWNER");

				if (script.mode == ScriptMode::jass) {
					script.call("TriggerAddAction", "t", "function ItemTable_" + std::to_string(i.item_table_pointer));
				} else {
					script.call("TriggerAddAction", "t", "ItemTable_" + std::to_string(i.item_table_pointer));
				}
			}
		}
	});
}

std::string generate_unit_script(
	const Terrain& terrain,
	const Units& units,
	const ScriptMode mode,
	const std::unordered_map<std::string, std::string>& unit_variables
) {
	MapScriptWriter script(mode);
	generate_units(script, unit_variables, terrain, units);
	return script.script;
}

void generate_items(
	MapScriptWriter& script,
	const std::unordered_map<std::string, std::string>& item_variables,
	const Terrain& terrain,
	const Units& units
) {
	script.function("CreateAllItems", [&]() {
		for (const auto& i : units.items) {
			const float x = i.position.x * 128.f + terrain.offset.x;
			const float y = i.position.y * 128.f + terrain.offset.y;

			// A reference whose type differs from the placed item names a global that is declared with the other type, so
			// assigning this one would write an undeclared global.
			const auto item_variable = item_variables.find(padded_creation_number(i.creation_number));
			if (item_variable != item_variables.end() && item_variable->second == i.id) {
				script.set_variable(
					item_global_name(i.id, i.creation_number),
					std::format("BlzCreateItemWithSkin({}, {}, {}, {})", script.four_cc(i.id), x, y, script.four_cc(i.id))
				);
			} else {
				script.call("BlzCreateItemWithSkin", script.four_cc(i.id), x, y, script.four_cc(i.id));
			}
		}
	});
}

std::string generate_item_script(
	const std::unordered_map<std::string, std::string>& item_variables,
	const Terrain& terrain,
	const Units& units,
	const ScriptMode mode
) {
	MapScriptWriter script(mode);
	generate_item_globals(script, item_variables);
	generate_items(script, item_variables, terrain, units);
	return script.script;
}

void generate_destructables(
	MapScriptWriter& script,
	const std::unordered_map<std::string, std::string>& destructable_variables,
	const Terrain& terrain,
	const Doodads& doodads
) {
	script.function("CreateAllDestructables", [&]() {
		script.local("destructable", "d", script.null());
		script.local("trigger", "t", script.null());
		script.local("real", "life", "0");

		for (const auto& i : doodads.doodads) {
			std::string id = "d";

			if (destructable_variables.contains(padded_creation_number(i.creation_number))) {
				id = destructable_global_name(i.id, i.creation_number);
			}

			if (id == "d" && i.item_sets.empty() && i.item_table_pointer == -1) {
				continue;
			}

			script.set_variable(
				id,
				std::format(
					"BlzCreateDestructableZWithSkin({}, {:.4f}, {:.4f}, {:.4f}, {}, {}, {}, {})",
					script.four_cc(i.id),
					i.position.x * 128.f + terrain.offset.x,
					i.position.y * 128.f + terrain.offset.y,
					i.final_position(terrain).z * 128.f,
					glm::degrees(i.angle),
					i.scale.x,
					i.variation,
					script.four_cc(i.skin_id)
				)
			);

			if (i.life != 100) {
				script.set_variable("life", "GetDestructableLife(" + id + ")");
				script.call("SetDestructableLife", id, std::to_string(i.life / 100.f) + " * life");
			}

			if (!i.item_sets.empty()) {
				script.set_variable("t", "CreateTrigger()");
				script.call("TriggerRegisterDeathEvent", "t", id);
				if (script.mode == ScriptMode::jass) {
					script.call("TriggerAddAction", "t", "function SaveDyingWidget");
				} else {
					script.call("TriggerAddAction", "t", "SaveDyingWidget");
				}
				if (script.mode == ScriptMode::jass) {
					script.call("TriggerAddAction", "t", "function DoodadItemDrops_" + std::to_string(i.creation_number));
				} else {
					script.call("TriggerAddAction", "t", "DoodadItemDrops_" + std::to_string(i.creation_number));
				}
			} else if (i.item_table_pointer != -1) {
				script.set_variable("t", "CreateTrigger()");
				script.call("TriggerRegisterDeathEvent", "t", id);
				if (script.mode == ScriptMode::jass) {
					script.call("TriggerAddAction", "t", "function SaveDyingWidget");
					script.call("TriggerAddAction", "t", "function ItemTable_" + std::to_string(i.item_table_pointer));
				} else {
					script.call("TriggerAddAction", "t", "SaveDyingWidget");
					script.call("TriggerAddAction", "t", "ItemTable_" + std::to_string(i.item_table_pointer));
				}
			}
		}
	});
}

void generate_regions(MapScriptWriter& script, const Regions& regions, const Terrain& terrain) {
	script.function("CreateRegions", [&]() {
		script.local("weathereffect", "we", script.null());
		for (const auto& i : regions.regions) {
			const std::string region_name = prefixed_script_identifier("gg_rct_", i.name);

			const auto to_wc3_x = [&](float x) {
				return x * 128.f + terrain.offset.x;
			};

			const auto to_wc3_y = [&](float y) {
				return y * 128.f + terrain.offset.y;
			};

			script.set_variable(
				region_name,
				std::format(
					"Rect({}, {}, {}, {})",
					to_wc3_x(std::min(i.left, i.right)),
					to_wc3_y(std::min(i.bottom, i.top)),
					to_wc3_x(std::max(i.left, i.right)),
					to_wc3_y(std::max(i.bottom, i.top))
				)
			);

			if (!i.weather_id.empty()) {
				script.set_variable("we", std::format("AddWeatherEffect({}, {})", region_name, script.four_cc(i.weather_id)));
				script.call("EnableWeatherEffect", "we", true);
			}
		}
	});
}

void generate_cameras(MapScriptWriter& script, const GameCameras& cameras) {
	script.function("CreateCameras", [&]() {
		for (const auto& i : cameras.cameras) {
			const std::string camera_name = prefixed_script_identifier("gg_cam_", i.name);

			script.set_variable(camera_name, "CreateCameraSetup()");
			script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_ZOFFSET", i.z_offset, 0.0);
			script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_ROTATION", i.rotation, 0.0);
			script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_ANGLE_OF_ATTACK", i.angle_of_attack, 0.0);
			script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_TARGET_DISTANCE", i.distance, 0.0);
			script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_ROLL", i.roll, 0.0);
			script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_FIELD_OF_VIEW", i.fov, 0.0);
			script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_FARZ", i.far_z, 0.0);
			script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_NEARZ", i.near_z, 0.0);
			script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_LOCAL_PITCH", i.local_pitch, 0.0);
			script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_LOCAL_YAW", i.local_yaw, 0.0);
			script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_LOCAL_ROLL", i.local_roll, 0.0);
			script.call("CameraSetupSetDestPosition", camera_name, i.target_x, i.target_y, 0.0);
		}
	});
}

// Real arguments of the sound setters: one decimal, as every stock script writes them (1.04 becomes 1.0, 100000 becomes 100000.0).
// The default "{}" formatting would write 100000.f as 1e+05, which JASS rejects.
std::string sound_real(const float value) {
	// Callers skip non-finite values (has_sound_value), which would print as nan or inf.
	return std::format("{:.1f}", value);
}

// A w3s float that gets a setter: not the 2^32 "default" and finite. NaN or infinity from a damaged file is treated as default.
bool has_sound_value(const float value) {
	return std::isfinite(value) && !is_default_sound_float(value);
}

// Call order matches the World Editor (checked against the 469 stock 3.0.1 scripts). Without a label: duration, channel, volume, pitch,
// and for 3D sounds distances, cutoff, cone angles, cone orientation. With a label the World Editor writes only the setters whose value
// differs from the label's SLK entry; HiveWE does not load those SLKs, so it writes duration and volume only and lets
// SetSoundParamsFromLabel supply the rest (a user override of a label value is therefore lost). The 2^32 "default" float and a -1
// channel get no setter.
void generate_sounds(MapScriptWriter& script, const Sounds& sounds, const SoundDurationLookup& duration) {
	const auto quoted = [](const std::string_view text) {
		return "\"" + string_replaced(std::string(text), "\\", "\\\\") + "\"";
	};

	script.function("InitSounds", [&]() {
		for (const auto& i : sounds.sounds) {
			const std::string sound_name = prefixed_script_identifier("", i.name);

			if (i.music) {
				// I suppose music files can't be muted?
				script.set_variable(sound_name, quoted(i.file));
				continue;
			}

			script.set_variable(
				sound_name,
				std::format(
					"CreateSound({}, {}, {}, {}, {}, {}, {})",
					quoted(i.file),
					i.looping ? "true" : "false",
					i.is_3d ? "true" : "false",
					i.stop_out_of_range ? "true" : "false",
					i.fade_in_rate,
					i.fade_out_rate,
					quoted(i.eax_effect)
				)
			);

			const bool has_label = !i.label.empty();
			if (has_label) {
				script.call("SetSoundParamsFromLabel", sound_name, quoted(i.label));
			}
			if (!i.facial_animation_label.empty()) {
				script.call("SetSoundFacialAnimationLabel", sound_name, quoted(i.facial_animation_label));
			}
			if (!i.facial_animation_group_label.empty()) {
				script.call("SetSoundFacialAnimationGroupLabel", sound_name, quoted(i.facial_animation_group_label));
			}
			if (!i.facial_animation_set_path.empty()) {
				script.call("SetSoundFacialAnimationSetFilepath", sound_name, quoted(i.facial_animation_set_path));
			}
			if (i.speaker_key != -1) {
				script.call("SetDialogueSpeakerNameKey", sound_name, quoted(std::format("TRIGSTR_{}", i.speaker_key)));
			}
			if (i.text_key != -1) {
				script.call("SetDialogueTextKey", sound_name, quoted(std::format("TRIGSTR_{}", i.text_key)));
			}

			if (const auto milliseconds = duration(i)) {
				script.call("SetSoundDuration", sound_name, *milliseconds);
			} else {
				std::println("InitSounds: could not determine the duration of {} ({}), SetSoundDuration omitted", i.name, i.file);
			}
			if (!has_label && i.channel != -1) {
				script.call("SetSoundChannel", sound_name, i.channel);
			}
			// Stock scripts write the volume even when it is -1.
			script.call("SetSoundVolume", sound_name, i.volume);
			if (has_label) {
				continue;
			}

			if (has_sound_value(i.pitch)) {
				script.call("SetSoundPitch", sound_name, sound_real(i.pitch));
			}
			if (!i.is_3d) {
				continue;
			}
			if (has_sound_value(i.min_distance) && has_sound_value(i.max_distance)) {
				script.call("SetSoundDistances", sound_name, sound_real(i.min_distance), sound_real(i.max_distance));
			}
			if (has_sound_value(i.distance_cutoff)) {
				script.call("SetSoundDistanceCutoff", sound_name, sound_real(i.distance_cutoff));
			}
			if (has_sound_value(i.cone_inside) && has_sound_value(i.cone_outside) && i.cone_outside_volume != -1) {
				script.call("SetSoundConeAngles", sound_name, sound_real(i.cone_inside), sound_real(i.cone_outside), i.cone_outside_volume);
			}
			if (has_sound_value(i.cone_orientation_x) && has_sound_value(i.cone_orientation_y)
				&& has_sound_value(i.cone_orientation_z)) {
				script.call(
					"SetSoundConeOrientation",
					sound_name,
					sound_real(i.cone_orientation_x),
					sound_real(i.cone_orientation_y),
					sound_real(i.cone_orientation_z)
				);
			}
		}
	});
}

/// SetSoundDuration for a sound: the play length of its audio file, resolved through the hierarchy (map files, then game data
/// including the locale folders where 3.0 dialogue lives). Game files do not change during a session, so their durations are cached
/// by path and music is read once, not on every save. A file the map itself holds is read each time, since the user may replace it.
std::optional<int> hierarchy_sound_duration(const Sound& sound) {
	static std::unordered_map<std::string, std::optional<int>> game_file_durations;

	const bool map_file = hierarchy.map_file_exists(sound.file);
	// The settings that change which game file open_file picks are part of the key, so a reload with another tileset or mode
	// cannot serve a stale duration.
	std::string key = std::format(
		"{}|{}|{}{}{}|{}",
		hierarchy.tileset,
		hierarchy.locale,
		hierarchy.hd ? 'h' : '-',
		hierarchy.teen ? 't' : '-',
		hierarchy.local_files ? 'l' : '-',
		to_lowercase_copy(sound.file)
	);
	std::ranges::replace(key, '\\', '/');
	if (!map_file) {
		if (const auto found = game_file_durations.find(key); found != game_file_durations.end()) {
			return found->second;
		}
	}

	std::optional<int> result;
	if (const auto file = hierarchy.open_file(sound.file)) {
		result = audio_duration_ms(file->buffer);
	}
	if (!map_file) {
		game_file_durations.emplace(std::move(key), result);
	}
	return result;
}

std::string generate_sound_script(const Sounds& sounds, const ScriptMode mode, const SoundDurationLookup& duration) {
	MapScriptWriter script(mode);
	generate_sounds(script, sounds, duration);
	return script.script;
}

void generate_trigger_initialization(
	MapScriptWriter& script,
	const std::vector<std::string>& initialization_triggers,
	const std::vector<Trigger>& triggers
) {
	script.function("InitCustomTriggers", [&]() {
		for (const auto& i : triggers) {
			if (!emits_trigger_identifier(i, script.mode)) {
				continue;
			}

			script.call(prefixed_script_identifier("InitTrig_", i.name));
		}
	});

	script.function("RunInitializationTriggers", [&]() {
		for (const auto& i : initialization_triggers) {
			script.call("ConditionalTriggerExecute", i);
		}
	});
}

std::string generate_trigger_initialization_script(
	const std::vector<std::string>& initialization_triggers,
	const std::vector<Trigger>& triggers,
	const ScriptMode mode
) {
	MapScriptWriter script(mode);
	generate_trigger_initialization(script, initialization_triggers, triggers);
	return script.script;
}

void generate_players(MapScriptWriter& script, const MapInfo& map_info) {
	script.function("InitCustomPlayerSlots", [&]() {
		const std::vector<std::string> players =
			{"MAP_CONTROL_USER", "MAP_CONTROL_COMPUTER", "MAP_CONTROL_NEUTRAL", "MAP_CONTROL_RESCUABLE"};
		const std::vector<std::string> races =
			{"RACE_PREF_RANDOM", "RACE_PREF_HUMAN", "RACE_PREF_ORC", "RACE_PREF_UNDEAD", "RACE_PREF_NIGHTELF"};

		const auto start_locations = start_location_indices(map_info);
		for (const auto& i : map_info.players) {
			const int index = start_locations.at(i.internal_number);
			std::string player = "Player(" + std::to_string(i.internal_number) + ")";

			// The w3i dword stored in fixed_start_position is a flag set: 0x1 fixed start location, 0x2 race selectable.
			script.call("SetPlayerStartLocation", player, index);
			if (i.fixed_start_position & 0x1) {
				script.call("ForcePlayerStartLocation", player, index);
			}

			const bool race_selectable =
				!map_info.fixed_player_settings || (i.fixed_start_position & 0x2) || i.race == PlayerRace::selectable;
			script.call("SetPlayerColor", player, "ConvertPlayerColor(" + std::to_string(i.internal_number) + ")");
			script.call("SetPlayerRacePreference", player, races[static_cast<int>(i.race)]);
			script.call("SetPlayerRaceSelectable", player, race_selectable);
			script.call("SetPlayerController", player, players[static_cast<int>(i.type)]);

			if (i.type == PlayerType::rescuable) {
				for (const auto& j : map_info.players) {
					if (j.type == PlayerType::human) {
						script.call(
							"SetPlayerAlliance",
							player,
							"Player(" + std::to_string(j.internal_number) + ")",
							"ALLIANCE_RESCUABLE",
							true
						);
					}
				}
			}

			script.write("\n");
		}
	});
}

std::string generate_player_slots_script(const MapInfo& map_info, const ScriptMode mode) {
	MapScriptWriter script(mode);
	generate_players(script, map_info);
	return script.script;
}

// SetTeams takes the player count like SetPlayers, and the placement depends only on the number of human slots (466/466 stock).
static void generate_map_settings(MapScriptWriter& script, const MapInfo& map_info) {
	const auto human_players =
		std::ranges::count_if(map_info.players, [](const PlayerData& player) { return player.type == PlayerType::human; });

	script.call("SetPlayers", map_info.players.size());
	script.call("SetTeams", map_info.players.size());
	script.call("SetGamePlacement", human_players > 1 ? "MAP_PLACEMENT_TEAMS_TOGETHER" : "MAP_PLACEMENT_USE_MAP_SETTINGS");
}

std::string generate_map_settings_script(const MapInfo& map_info, const ScriptMode mode) {
	MapScriptWriter script(mode);
	generate_map_settings(script, map_info);
	return script.script;
}

static void write_item_table_entry(MapScriptWriter& script, int chance, const std::string& id) {
	if (id.empty()) {
		script.call("RandomDistAddItem", -1, chance);
	} else if (id[0] == 'Y' && id[2] == 'I' && ((id[1] >= 'i' && id[1] <= 'o') || id[1] == 'Y')) { // Random items

		std::string item_type;
		switch (id[1]) {
			case 'i':
				item_type = "PERMANENT";
				break;
			case 'j':
				item_type = "CHARGED";
				break;
			case 'k':
				item_type = "POWERUP";
				break;
			case 'l':
				item_type = "ARTIFACT";
				break;
			case 'm':
				item_type = "PURCHASABLE";
				break;
			case 'n':
				item_type = "CAMPAIGN";
				break;
			case 'o':
				item_type = "MISCELLANEOUS";
				break;
			case 'Y':
				item_type = "ANY";
				break;
			default:
				std::println("Error: unknown random item type {}", id[1]);
		}

		const std::string random_item =
			std::format("ChooseRandomItemEx(ITEM_TYPE_{}, {})", item_type, (id[3] == '/') ? "-1" : std::string(1, id[3]));
		script.call("RandomDistAddItem", random_item, chance);
	} else {
		script.call("RandomDistAddItem", script.four_cc(id), chance);
	}
}

template<typename T>
void generate_item_tables(MapScriptWriter& script, const std::string& table_name_prefix, const std::vector<T>& table_holders) {
	for (const auto& i : table_holders) {
		if (i.item_sets.empty()) {
			continue;
		}

		script.function(table_name_prefix + std::to_string(i.creation_number), [&]() {
			script.local("widget", "trigWidget", script.null());
			script.local("unit", "trigUnit", script.null());
			script.local("integer", "itemID", "0");
			script.local("boolean", "canDrop", "true");

			script.set_variable("trigWidget", "bj_lastDyingWidget");

			script.if_statement("trigWidget == " + script.null(), [&] {
				script.set_variable("trigUnit", "GetTriggerUnit()");
			});

			script.if_statement("not(trigUnit == " + script.null() + ")", [&]() {
				script.set_variable("canDrop", "not IsUnitHidden(trigUnit)");
				script.if_statement("canDrop and not(GetChangingUnit() == " + script.null() + ")", [&]() {
					script.set_variable("canDrop", "(GetChangingUnitPrevOwner() == Player(PLAYER_NEUTRAL_AGGRESSIVE))");
				});
			});

			script.if_statement("canDrop", [&]() {
				for (const auto& j : i.item_sets) {
					script.call("RandomDistReset");
					for (const auto& [chance, id] : j.items) {
						write_item_table_entry(script, chance, id);
					}
					script.set_variable("itemID", "RandomDistChoose()");
					script.if_statement("not(trigUnit == " + script.null() + ")", [&]() {
						script.call("UnitDropItem", "trigUnit", "itemID"); // Todo fourcc?
					});
					script.if_statement("trigUnit == " + script.null(), [&]() {
						script.call("WidgetDropItem", "trigWidget", "itemID"); // Todo fourcc?
					});
				}
			});

			script.set_variable("bj_lastDyingWidget", script.null());
			script.call("DestroyTrigger", "GetTriggeringTrigger()");
		});
	}
}

void generate_custom_teams(MapScriptWriter& script, const MapInfo& map_info) {
	script.function("InitCustomTeams", [&]() {
		int current_force = 0;
		for (const auto& i : map_info.forces) {
			for (const auto& j : map_info.players) {
				if (i.player_masks & (1 << j.internal_number)) {
					script.call("SetPlayerTeam", "Player(" + std::to_string(j.internal_number) + ")", current_force);

					if (i.allied_victory) {
						script
							.call("SetPlayerState", "Player(" + std::to_string(j.internal_number) + ")", "PLAYER_STATE_ALLIED_VICTORY", 1);
					}
				}
			}

			for (const auto& j : map_info.players) {
				if (i.player_masks & (1 << j.internal_number)) {
					for (const auto& k : map_info.players) {
						if (i.player_masks & (1 << k.internal_number) && j.internal_number != k.internal_number) {
							if (i.allied) {
								script.call(
									"SetPlayerAllianceStateAllyBJ",
									"Player(" + std::to_string(j.internal_number) + ")",
									"Player(" + std::to_string(k.internal_number) + ")",
									true
								);
							}
							if (i.share_vision) {
								script.call(
									"SetPlayerAllianceStateVisionBJ",
									"Player(" + std::to_string(j.internal_number) + ")",
									"Player(" + std::to_string(k.internal_number) + ")",
									true
								);
							}
							if (i.share_unit_control) {
								script.call(
									"SetPlayerAllianceStateControlBJ",
									"Player(" + std::to_string(j.internal_number) + ")",
									"Player(" + std::to_string(k.internal_number) + ")",
									true
								);
							}
							if (i.share_advanced_unit_control) {
								script.call(
									"SetPlayerAllianceStateFullControlBJ",
									"Player(" + std::to_string(j.internal_number) + ")",
									"Player(" + std::to_string(k.internal_number) + ")",
									true
								);
							}
						}
					}
				}
			}
			current_force++;
		}
	});
}

void generate_ally_priorities(MapScriptWriter& script, const MapInfo& map_info) {
	script.function("InitAllyPriorities", [&]() {
		std::unordered_map<int, int> player_to_startloc = start_location_indices(map_info);

		int current_player = 0;
		for (const auto& i : map_info.players) {
			size_t count = 0;
			for (const auto& j : map_info.players) {
				if (i.ally_low_priorities_flags & (1 << j.internal_number) && i.internal_number != j.internal_number) {
					count++;
				} else if (i.ally_high_priorities_flags & (1 << j.internal_number) && i.internal_number != j.internal_number) {
					count++;
				}
			}

			script.call("SetStartLocPrioCount", current_player, count);

			size_t current_index = 0;
			for (const auto& j : map_info.players) {
				if (i.ally_low_priorities_flags & (1 << j.internal_number) && i.internal_number != j.internal_number) {
					script
						.call("SetStartLocPrio", current_player, current_index, player_to_startloc[j.internal_number], "MAP_LOC_PRIO_LOW");
					current_index++;
				} else if (i.ally_high_priorities_flags & (1 << j.internal_number) && i.internal_number != j.internal_number) {
					script
						.call("SetStartLocPrio", current_player, current_index, player_to_startloc[j.internal_number], "MAP_LOC_PRIO_HIGH");
					current_index++;
				}
			}

			current_player++;
		}
	});
}

void generate_main(MapScriptWriter& script, const Terrain& terrain, const MapInfo& map_info) {
	script.function("main", [&]() {
		script.call(
			"SetCameraBounds",
			std::to_string(map_info.camera_left_bottom.x - 512.f) + " + GetCameraMargin(CAMERA_MARGIN_LEFT)",
			std::to_string(map_info.camera_left_bottom.y - 256.f) + " + GetCameraMargin(CAMERA_MARGIN_BOTTOM)",

			std::to_string(map_info.camera_right_top.x + 512.f) + " - GetCameraMargin(CAMERA_MARGIN_RIGHT)",
			std::to_string(map_info.camera_right_top.y + 256.f) + " - GetCameraMargin(CAMERA_MARGIN_TOP)",

			std::to_string(map_info.camera_left_top.x - 512.f) + " + GetCameraMargin(CAMERA_MARGIN_LEFT)",
			std::to_string(map_info.camera_left_top.y + 256.f) + " - GetCameraMargin(CAMERA_MARGIN_TOP)",

			std::to_string(map_info.camera_right_bottom.x + 512.f) + " - GetCameraMargin(CAMERA_MARGIN_RIGHT)",
			std::to_string(map_info.camera_right_bottom.y - 256.f) + " + GetCameraMargin(CAMERA_MARGIN_BOTTOM)"
		);

		const std::string terrain_lights = string_replaced(world_edit_data.data("TerrainLights", ""s + terrain.tileset), "\\", "/");
		const std::string unit_lights = string_replaced(world_edit_data.data("TerrainLights", ""s + terrain.tileset), "\\", "/");
		script.call("SetDayNightModels", "\"" + terrain_lights + "\"", "\"" + unit_lights + "\"");

		const std::string sound_environment = string_replaced(world_edit_data.data("SoundEnvironment", ""s + terrain.tileset), "\\", "/");
		script.call("NewSoundEnvironment", "\"" + sound_environment + "\"");

		const std::string ambient_day = string_replaced(world_edit_data.data("DayAmbience", ""s + terrain.tileset), "\\", "/");
		script.call("SetAmbientDaySound", "\"" + ambient_day + "\"");

		const std::string ambient_night = string_replaced(world_edit_data.data("NightAmbience", ""s + terrain.tileset), "\\", "/");
		script.call("SetAmbientNightSound", "\"" + ambient_night + "\"");

		script.call("SetMapMusic", "\"Music\"", true, 0);
		script.call("InitSounds");
		script.call("CreateRegions");
		script.call("CreateCameras");
		script.call("CreateAllDestructables");
		script.call("CreateAllItems");
		script.call("CreateAllUnits");
		script.call("InitBlizzard");
		script.call("InitGlobals");
		script.call("InitCustomTriggers");
		script.call("RunInitializationTriggers");
	});
}

void generate_map_configuration(MapScriptWriter& script, const Terrain& terrain, const Units& units, const MapInfo& map_info) {
	script.function("config", [&]() {
		script.call("SetMapName", "\"" + map_info.name + "\"");
		script.call("SetMapDescription", "\"" + map_info.description + "\"");
		generate_map_settings(script, map_info);

		script.write("\n");

		const auto start_locations = start_location_indices(map_info);
		for (const auto& player : map_info.players) {
			const Unit* start_location = units.find_start_location(player.internal_number);
			if (!start_location) {
				continue;
			}

			script.call(
				"DefineStartLocation",
				start_locations.at(player.internal_number),
				start_location->position.x * 128.f + terrain.offset.x,
				start_location->position.y * 128.f + terrain.offset.y
			);
		}
		script.write("\n");

		script.call("InitCustomPlayerSlots");
		if (map_info.custom_forces) {
			script.call("InitCustomTeams");
		} else {
			for (const auto& i : map_info.players) {
				script.call("SetPlayerSlotAvailable", "Player(" + std::to_string(i.internal_number) + ")", "MAP_CONTROL_USER");
			}

			script.call("InitGenericPlayerSlots");
		}
		script.call("InitAllyPriorities");
	});
}

/// Returns compile output which could contain errors or general information
std::expected<void, std::string> Triggers::generate_map_script(
	const Terrain& terrain,
	const Units& units,
	const Doodads& doodads,
	const MapInfo& map_info,
	const Sounds& sounds,
	const Regions& regions,
	const GameCameras& cameras,
	ScriptMode mode
) const {
	std::unordered_map<std::string, std::string> unit_variables; // creation_number, unit_id
	std::unordered_map<std::string, std::string> destructable_variables; // creation_number, destructable_id
	std::unordered_map<std::string, std::string> item_variables; // creation_number, item_id
	std::vector<std::string> initialization_triggers;

	std::string trigger_script;
	for (const auto& i : triggers) {
		if (i.is_comment || !i.is_enabled) {
			continue;
		}
		// A Lua script element (classifier 32) gets no InitTrig_ call, so it is not converted from GUI even with empty text
		// (0239 "Cursed Flute"). JASS calls InitTrig_ for it, so the GUI path must still define one there.
		if (!i.custom_text.empty() || (mode == ScriptMode::lua && i.classifier == Classifier::script)) {
			append_custom_text_trigger(trigger_script, initialization_triggers, i, mode);
		} else {
			trigger_script += convert_gui_to_jass(i, initialization_triggers, mode);
		}
	}

	// Search the trigger script for global unit/destructible definitions
	size_t pos = trigger_script.find("gg_unit", 0);
	while (pos != std::string::npos) {
		std::string type = trigger_script.substr(pos + 8, 4);
		std::string creation_number = trigger_script.substr(pos + 13, 4);
		unit_variables[creation_number] = type;
		pos = trigger_script.find("gg_unit", pos + 17);
	}

	pos = trigger_script.find("gg_dest", 0);
	while (pos != std::string::npos) {
		std::string type = trigger_script.substr(pos + 8, 4);
		std::string creation_number = trigger_script.substr(pos + 13, trigger_script.find_first_not_of("0123456789", pos + 13) - pos - 13);
		destructable_variables[creation_number] = type;
		pos = trigger_script.find("gg_dest", pos + 17);
	}

	item_variables = scan_item_variables(trigger_script);

	MapScriptWriter script_writer(mode);

	generate_global_variables(
		script_writer,
		unit_variables,
		item_variables,
		destructable_variables,
		triggers,
		variables,
		trigger_data,
		regions,
		cameras,
		sounds
	);
	generate_init_global_variables(script_writer, variables, trigger_data);
	generate_item_tables(script_writer, "ItemTable_", map_info.random_item_tables);
	generate_item_tables(script_writer, "UnitItemDrops_", units.units);
	generate_item_tables(script_writer, "DoodadItemDrops_", doodads.doodads);
	generate_sounds(script_writer, sounds, hierarchy_sound_duration);

	generate_destructables(script_writer, destructable_variables, terrain, doodads);
	generate_items(script_writer, item_variables, terrain, units);
	generate_units(script_writer, unit_variables, terrain, units);
	generate_regions(script_writer, regions, terrain);
	generate_cameras(script_writer, cameras);

	script_writer.write_ln(global_jass);

	script_writer.write(trigger_script);

	generate_trigger_initialization(script_writer, initialization_triggers, triggers);
	generate_players(script_writer, map_info);
	generate_custom_teams(script_writer, map_info);
	generate_ally_priorities(script_writer, map_info);
	generate_main(script_writer, terrain, map_info);
	generate_map_configuration(script_writer, terrain, units, map_info);

	fs::path path = QDir::tempPath().toStdString() + "/input.lua";
	std::ofstream output(path, std::ios::binary);
	output.write((char*)script_writer.script.data(), script_writer.script.size());
	output.close();

	if (mode == ScriptMode::jass) {
		QProcess* proc = new QProcess();
		proc->setWorkingDirectory("data/tools");
		proc->start(
			"data/tools/clijasshelper.exe",
			{"--scriptonly", "common.j", "blizzard.j", QString::fromStdString(path.string()), "war3map.j"}
		);
		proc->waitForFinished();
		const QString result = proc->readAllStandardOutput();

		if (result.contains("Compile error")) {
			return std::unexpected(result.mid(result.indexOf("Compile error")).toStdString());
		} else if (result.contains("compile errors")) {
			return std::unexpected(result.mid(result.indexOf("compile errors.")).toStdString());
		} else {
			hierarchy.map_file_add("data/tools/war3map.j", "war3map.j");
		}
	} else {
		hierarchy.map_file_add(path, "war3map.lua");
	}

	return {};
}
