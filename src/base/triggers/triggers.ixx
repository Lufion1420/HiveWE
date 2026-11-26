export module Triggers;

import std;
import Hierarchy;
import Utilities;
import Globals;
import <glm/glm.hpp>;
import Units;
import Doodads;
import Regions;
import GameCameras;
import Sounds;
import Terrain;
import MapInfo;
import BinaryReader;
import BinaryWriter;
import INI;
import LuaSyntax;

namespace fs = std::filesystem;
using namespace std::literals::string_literals;

export enum class ScriptMode {
	lua,
	jass
};

/// Creation numbers in gg_unit_/gg_dest_ names are zero padded to at least 4 digits ("0144"); longer numbers stay as is.
export std::string padded_creation_number(const int creation_number) {
	return std::format("{:0>4}", creation_number);
}

export std::string destructable_global_name(const std::string_view id, const int creation_number) {
	return std::format("gg_dest_{}_{}", id, padded_creation_number(creation_number));
}

export std::string item_global_name(const std::string_view id, const int creation_number) {
	return std::format("gg_item_{}_{}", id, padded_creation_number(creation_number));
}

/// Collects every gg_item_<id>_<number> the trigger script references, keyed by the number's digits exactly as written (no
/// padding is added) with the 4-char item id as value. A match whose id holds a character outside [A-Za-z0-9_] is skipped.
/// Only these items get a global, like units.
export std::unordered_map<std::string, std::string> scan_item_variables(const std::string_view trigger_script) {
	std::unordered_map<std::string, std::string> result;
	constexpr std::string_view prefix = "gg_item_";
	size_t pos = trigger_script.find(prefix);
	while (pos != std::string_view::npos) {
		const size_t number_start = pos + 13;
		const auto valid_type = [&] {
			for (const char c : trigger_script.substr(pos + 8, 4)) {
				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
					return false;
				}
			}
			return true;
		};
		if (number_start <= trigger_script.size() && trigger_script[pos + 12] == '_' && valid_type()) {
			const size_t number_end = std::min(trigger_script.find_first_not_of("0123456789", number_start), trigger_script.size());
			if (number_end > number_start) {
				result[std::string(trigger_script.substr(number_start, number_end - number_start))] =
					std::string(trigger_script.substr(pos + 8, 4));
			}
		}
		pos = trigger_script.find(prefix, pos + prefix.size());
	}
	return result;
}

/// Test seam: the item globals and CreateAllItems as generate_map_script writes them for the given referenced items.
export std::string generate_item_script(
	const std::unordered_map<std::string, std::string>& item_variables,
	const Terrain& terrain,
	const Units& units,
	ScriptMode mode
);

/// Test seam: CreateAllUnits as generate_map_script writes it. `unit_variables` maps a zero-padded creation number to the unit id
/// referenced by trigger text (empty: no unit globals). Ability orders come from abilities_slk, colours and gold mines from units_slk.
export std::string generate_unit_script(
	const Terrain& terrain,
	const Units& units,
	ScriptMode mode,
	const std::unordered_map<std::string, std::string>& unit_variables = {}
);

/// Returns the play length in milliseconds of a sound's audio file, or nullopt when it cannot be determined.
export using SoundDurationLookup = std::function<std::optional<int>(const Sound&)>;

/// Test seam: InitSounds as generate_map_script writes it, with durations taken from `duration`.
export std::string generate_sound_script(const Sounds& sounds, ScriptMode mode, const SoundDurationLookup& duration);

/// The game numbers start locations 0..n-1 in map_info.players order, not by player number. DefineStartLocation,
/// SetPlayerStartLocation and SetStartLocPrio must all use this mapping. A player without a start location unit still
/// owns its index (the index is its position in the list).
export std::unordered_map<int, int> start_location_indices(const MapInfo& map_info) {
	std::unordered_map<int, int> result;
	int index = 0;
	for (const auto& player : map_info.players) {
		result[player.internal_number] = index++;
	}
	return result;
}

/// Turns a trigger, region, camera or sound name into the identifier part used in generated scripts: every character that is
/// not [A-Za-z0-9_] (spaces, apostrophes, '=', non-ASCII bytes) becomes '_'. Callers trim the name first.
export std::string script_identifier(std::string name) {
	for (char& c : name) {
		const bool valid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
		if (!valid) {
			c = '_';
		}
	}
	return name;
}

/// Joins an event's resolved parameters into the call arguments after the trigger variable. An event without parameters
/// yields just the trigger variable, with no trailing separator.
export std::string event_call_arguments(const std::string_view trigger_variable, const std::string_view parameters) {
	if (parameters.empty()) {
		return std::string(trigger_variable);
	}
	return std::string(trigger_variable) + ", " + std::string(parameters);
}

/// TriggerData stores operator presets quoted (`"!="`) and null handle presets as `null`. Script output needs the bare
/// operator, and `nil` in Lua. `preset_type` is the preset's TriggerParams type (e.g. EqualNotEqualOperator).
export std::string normalize_preset_value(const std::string_view preset_type, std::string value, const ScriptMode mode) {
	if (preset_type.ends_with("Operator") && value.size() >= 2 && value.front() == '"' && value.back() == '"') {
		value = value.substr(1, value.size() - 2);
	}
	if (mode == ScriptMode::lua) {
		if (value == "null") {
			value = "nil";
		} else if (value == "!=") {
			value = "~=";
		}
	}
	return value;
}

/// Initial value the World Editor writes for a GUI variable in war3map.lua. `base_type` is the resolved base type (custom types such as
/// unitcode already mapped to integer). Arrays of value types need a __jarray default so unset slots read as 0/false/"" instead of nil;
/// handle arrays are plain tables. Scalar handles are nil.
export std::string lua_global_default(const std::string_view base_type, const bool is_array) {
	std::string value;
	if (base_type == "integer") {
		value = "0";
	} else if (base_type == "real") {
		value = "0.0";
	} else if (base_type == "boolean") {
		value = "false";
	} else if (base_type == "string") {
		value = "\"\"";
	} else {
		return is_array ? "{}" : "nil";
	}
	return is_array ? "__jarray(" + value + ")" : value;
}

/// A minimal utility wrapper around a std::string that manages newlines, indentation and closing braces
struct MapScriptWriter {
	std::string script;
	size_t current_indentation = 0;

	ScriptMode mode;

	explicit MapScriptWriter(ScriptMode mode = ScriptMode::lua) : mode(mode) {}

	bool is_empty() {
		return script.empty();
	}

	void merge(const MapScriptWriter& writer) {
		script += writer.script;
	}

	void raw_write_to_log(const std::string_view users_fmt, std::format_args&& args) {
		std::vformat_to(std::back_inserter(script), users_fmt, args);
	}

	constexpr void local(std::string_view type, std::string_view name, std::string_view value) {
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}

		if (mode == ScriptMode::lua) {
			std::format_to(std::back_inserter(script), "local {} = {}\n", name, value);
		} else {
			std::format_to(std::back_inserter(script), "local {} {} = {}\n", type, name, value);
		}
	}

	constexpr void global(std::string_view type, std::string_view name, std::string_view value) {
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}

		if (mode == ScriptMode::lua) {
			std::format_to(std::back_inserter(script), "{} = {}\n", name, value);
		} else {
			std::format_to(std::back_inserter(script), "{} {} = {}\n", type, name, value);
		}
	}

	constexpr void set_variable(std::string_view name, std::string_view value) {
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}

		if (mode == ScriptMode::lua) {
			std::format_to(std::back_inserter(script), "{} = {}\n", name, value);
		} else {
			std::format_to(std::back_inserter(script), "set {} = {}\n", name, value);
		}
	}

	template<typename... Args>
	constexpr void inline_call(std::string_view name, Args&&... args) {
		std::string work = "{}(";

		for (size_t i = 0; i < sizeof...(args); i++) {
			work += "{}";
			if (i < sizeof...(args) - 1) {
				work += ", ";
			}
		}
		work += ")";
		// Reduce binary code size by having only one instantiation
		raw_write_to_log(work, std::make_format_args(name, args...));
	}

	template<typename... Args>
	constexpr void call(std::string_view name, Args&&... args) {
		std::string work;

		if (mode == ScriptMode::jass) {
			work = "call {}(";
		} else {
			work = "{}(";
		}

		for (size_t i = 0; i < sizeof...(args); i++) {
			work += "{}";
			if (i < sizeof...(args) - 1) {
				work += ", ";
			}
		}
		work += ")\n";

		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}
		// Reduce binary code size by having only one instantiation
		raw_write_to_log(work, std::make_format_args(name, args...));
	}

	void write(const std::string_view string) {
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}

		script += string;
	}

	constexpr void write_ln(const std::string_view string) {
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}
		script += string;
		script += '\n';
	}

	template<typename... Args>
	constexpr void write_ln(Args&&... args) {
		std::string work = std::string(current_indentation, '\t');
		for (size_t i = 0; i < sizeof...(args); i++) {
			work += "{}";
		}
		work.push_back('\n');

		// Reduce binary code size by having only one instantiation
		raw_write_to_log(work, std::make_format_args(args...));
	}

	template<typename T>
	constexpr void forloop(size_t start, size_t end, const std::string_view variable, T callback) {
		if (mode == ScriptMode::jass) {
			set_variable(variable, std::to_string(start));
			write_ln("loop");
			current_indentation += 1;
			for (size_t i = 0; i < current_indentation; i++) {
				script += '\t';
			}
			std::format_to(std::back_inserter(script), "exitwhen {} > {}\n", variable, end);
		} else {
			for (size_t i = 0; i < current_indentation; i++) {
				script += '\t';
			}
			std::format_to(std::back_inserter(script), "for i={},{} do\n", start, end);
			current_indentation += 1;
		}

		callback();
		current_indentation -= 1;

		if (mode == ScriptMode::jass) {
			for (size_t i = 0; i < current_indentation; i++) {
				script += '\t';
			}
			std::format_to(std::back_inserter(script), "set {} = {} + 1\n", variable, variable);
			write_ln("endloop");
		} else {
			write_ln("end");
		}

	}

	template<typename T>
	void function(std::string_view name, T callback, const std::string_view return_type = "takes nothing returns nothing") {
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}

		if (mode == ScriptMode::lua) {
			std::format_to(std::back_inserter(script), "function {}()\n", name);
		} else {
			std::format_to(std::back_inserter(script), "function {} {}\n", name, return_type);
		}

		current_indentation += 1;
		callback();
		current_indentation -= 1;

		if (mode == ScriptMode::lua) {
			write("end\n\n");
		} else {
			write("endfunction\n\n");
		}
	}

	template<typename T>
	void while_statement(std::string_view condition, T callback) {
		if (mode == ScriptMode::jass) {
			write_ln("loop");
		} else {
			for (size_t i = 0; i < current_indentation; i++) {
				script += '\t';
			}

			std::format_to(std::back_inserter(script), "while ({}) do\n", condition);
		}

		current_indentation += 1;

		if (mode == ScriptMode::jass) {
			for (size_t i = 0; i < current_indentation; i++) {
				script += '\t';
			}
			std::format_to(std::back_inserter(script), "exitwhen (not ({}))\n", condition);
		}

		callback();
		current_indentation -= 1;

		if (mode == ScriptMode::jass) {
			write_ln("endloop");
		} else {
			write_ln("end");
		}
	}

	template<typename T>
	void if_statement(std::string_view condition, T callback) {
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}

		std::format_to(std::back_inserter(script), "if ({}) then\n", condition);

		current_indentation += 1;
		callback();
		current_indentation -= 1;

		if (mode == ScriptMode::lua) {
			write_ln("end");
		} else {
			write_ln("endif");
		}
	}

	template<typename T1, typename T2>
	void if_else_statement(std::string_view condition, T1 if_callback, T2 else_callback) {
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}

		std::format_to(std::back_inserter(script), "if ({}) then\n", condition);

		current_indentation += 1;
		if_callback();
		current_indentation -= 1;

		write("else\n");

		current_indentation += 1;
		else_callback();
		current_indentation -= 1;

		if (mode == ScriptMode::lua) {
			write_ln("end");
		} else {
			write_ln("endif");
		}
	}

	template<typename T>
	void global_variable(std::string_view name, T value) {
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}
		std::format_to(std::back_inserter(script), "udg_{} = {}", name, value);
	}

	/// The ID should not be quoted
	std::string four_cc(std::string_view id) {
		if (mode == ScriptMode::lua) {
			return std::format("FourCC(\"{}\")", id);
		} else {
			return std::format("'{}'", id);
		}
	}

	std::string null() {
		if (mode == ScriptMode::lua) {
			return "nil";
		} else {
			return "null";
		}
	}
};

export enum class Classifier {
	map = 1,
	library = 2,
	category = 4,
	gui = 8,
	comment = 16,
	script = 32,
	variable = 64
};

export struct TriggerCategory {
	Classifier classifier = Classifier::category;
	int id = 0;
	std::string name;
	bool open_state = true;
	bool is_comment = false;
	int parent_id = 0;
};

/// A category as the Trigger Editor creates it.
export TriggerCategory make_new_trigger_category(std::string name, const int id, const int parent_id) {
	TriggerCategory category;
	category.classifier = Classifier::category;
	category.name = std::move(name);
	category.id = id;
	category.parent_id = parent_id;
	return category;
}

/// The wtg's entry for the map header. The Trigger Explorer shows it as its own fixed item, not as a category.
export bool is_map_header_entry(const TriggerCategory& category) {
	return category.parent_id == -1 && (category.classifier == Classifier::map || category.id == 0);
}

/// Where the Trigger Explorer lists each entry of `categories`, by index: the index of its parent category, or nullopt for the map
/// header (also for the header's own entry). Parents may be stored in any order. A category whose parent is missing, and the first one
/// (in vector order) of a parent cycle, are listed under the map header, so every category can be reached.
export std::vector<std::optional<std::size_t>> category_tree_parents(const std::vector<TriggerCategory>& categories) {
	std::unordered_map<int, std::vector<std::size_t>> indices_by_id;
	for (std::size_t i = 0; i < categories.size(); i++) {
		if (!is_map_header_entry(categories[i])) {
			indices_by_id[categories[i].id].push_back(i);
		}
	}

	std::vector<std::optional<std::size_t>> parents(categories.size());
	for (std::size_t i = 0; i < categories.size(); i++) {
		if (is_map_header_entry(categories[i]) || categories[i].parent_id == 0) {
			continue;
		}
		const auto found = indices_by_id.find(categories[i].parent_id);
		if (found == indices_by_id.end()) {
			continue;
		}
		// With a duplicated id: the nearest one stored before the child, else the first one after it.
		for (const std::size_t candidate : found->second) {
			if (candidate < i || (candidate > i && !parents[i])) {
				parents[i] = candidate;
			}
			if (candidate > i) {
				break;
			}
		}
	}

	enum class State {
		unvisited,
		on_path,
		done
	};
	std::vector<State> states(categories.size(), State::unvisited);
	std::vector<std::size_t> path;
	for (std::size_t i = 0; i < categories.size(); i++) {
		path.clear();
		std::optional<std::size_t> at = i;
		while (at && states[*at] == State::unvisited) {
			states[*at] = State::on_path;
			path.push_back(*at);
			at = parents[*at];
		}
		if (at && states[*at] == State::on_path) { // the walk ran into itself
			parents[*std::min_element(std::ranges::find(path, *at), path.end())].reset();
		}
		for (const std::size_t index : path) {
			states[index] = State::done;
		}
	}
	return parents;
}

export struct TriggerParameter;

export struct ECA {
	enum class Type {
		event,
		condition,
		action,
		call
	};

	Type type;
	int group;
	std::string name;
	bool enabled;
	std::vector<TriggerParameter> parameters;
	std::vector<ECA> ecas;
};

export struct TriggerParameter {
	enum class Type {
		invalid = -1,
		preset,
		variable,
		function,
		string
	};
	Type type;
	int unknown;
	/// Unused when has_sub_parameter is true, sometimes contains garbage
	std::string value;
	bool has_sub_parameter;
	ECA sub_parameter;
	/// Only used when Type::function and has_sub_parameter=true
	// TriggerSubParameter sub_parameter;
	/// Only used when Type::variable
	bool is_array = false;
	/// Only used when Type::variable and is_array=true
	std::vector<TriggerParameter> parameters;
};

export struct Trigger {
	Classifier classifier = Classifier::gui;
	int id = 0;
	int parent_id = 0;
	std::string name;
	std::string description;
	std::string custom_text;
	bool is_comment = false;
	bool is_enabled = true;
	bool is_script = false;
	bool initially_on = true;
	bool run_on_initialization = false;
	std::vector<ECA> ecas;

	static inline int next_id = 0;
};

/// A GUI trigger, custom-text script or comment as the Trigger Editor creates it. A comment gets is_comment set, as the
/// World Editor writes for classifier 16 (139 of 139 stock comments).
export Trigger make_new_trigger(const Classifier classifier, std::string name, const int id, const int parent_id) {
	Trigger trigger;
	trigger.classifier = classifier;
	trigger.name = std::move(name);
	trigger.id = id;
	trigger.parent_id = parent_id;
	trigger.is_script = classifier == Classifier::script;
	trigger.is_comment = classifier == Classifier::comment;
	return trigger;
}

/// Whether a trigger gets a `gg_trg_` global and an `InitTrig_` call. Disabled triggers and comments never do. In Lua the
/// World Editor emits neither for script elements (wtg classifier 32, plain library code), whatever their text, but does for
/// a GUI trigger converted to custom text (classifier 8): stock 0239 Forgotten Hollow and 0240 Silverpine Sprint. JASS keeps
/// both for every enabled trigger.
export bool emits_trigger_identifier(const Trigger& trigger, const ScriptMode mode) {
	if (trigger.is_comment || !trigger.is_enabled) {
		return false;
	}
	return !(mode == ScriptMode::lua && trigger.classifier == Classifier::script);
}

/// prefix + the trimmed, sanitized name, e.g. gg_rct_ for a region. Used for both the global and every use of it.
export std::string prefixed_script_identifier(const std::string_view prefix, std::string name) {
	trim(name);
	return std::string(prefix) + script_identifier(std::move(name));
}

/// Appends an enabled trigger's custom text to the trigger script. A custom-text trigger marked "run on map initialization"
/// is added to `initialization_triggers`. For a Lua trigger that gets an InitTrig_ call, an empty InitTrig_ is written first,
/// as the World Editor does (0240), so the call works even when the text defines none; the text's own definition replaces it.
export void append_custom_text_trigger(
	std::string& trigger_script,
	std::vector<std::string>& initialization_triggers,
	const Trigger& trigger,
	const ScriptMode mode
) {
	const bool emits_identifier = emits_trigger_identifier(trigger, mode);
	if (mode == ScriptMode::lua && emits_identifier) {
		trigger_script += "function " + prefixed_script_identifier("InitTrig_", trigger.name) + "()\nend\n\n";
	}
	trigger_script += trigger.custom_text + "\n";
	if (emits_identifier && trigger.run_on_initialization) {
		initialization_triggers.push_back(prefixed_script_identifier("gg_trg_", trigger.name));
	}
}

/// Test seam: InitCustomTriggers and RunInitializationTriggers as generate_map_script writes them.
export std::string generate_trigger_initialization_script(
	const std::vector<std::string>& initialization_triggers,
	const std::vector<Trigger>& triggers,
	ScriptMode mode
);

/// Test seam: the SetPlayers, SetTeams and SetGamePlacement calls at the start of config, as generate_map_script writes them.
export std::string generate_map_settings_script(const MapInfo& map_info, ScriptMode mode);

/// Test seam: InitCustomPlayerSlots as generate_map_script writes it.
export std::string generate_player_slots_script(const MapInfo& map_info, ScriptMode mode);

/// Whether the World Editor writes InitAllyPriorities, and config calls it: some player has an ally or enemy priority bit for a
/// player in the list (its own bit counts).
export bool has_ally_priorities(const MapInfo& map_info);

/// Test seam: InitAllyPriorities as generate_map_script writes it, or nothing when has_ally_priorities() is false.
export std::string generate_ally_priorities_script(const MapInfo& map_info, ScriptMode mode);

/// Test seam: the SetWaterBaseColor call main() writes after SetDayNightModels, or nothing when water tinting is off.
export std::string generate_water_base_color_script(const MapInfo& map_info, ScriptMode mode);

/// Test seam: InitCustomTeams as generate_map_script writes it.
export std::string generate_custom_teams_script(const MapInfo& map_info, ScriptMode mode);

/// The bit of a player number in w3i player masks, or 0 for a number outside 0 to 31 (scenario_data_io defaults it to -1).
export uint32_t player_mask_bit(int internal_number);

/// Test seam: main() as generate_map_script writes it.
export std::string generate_main_script(const Terrain& terrain, const MapInfo& map_info, ScriptMode mode);

/// Test seam: config() as generate_map_script writes it.
export std::string generate_map_configuration_script(const Terrain& terrain, const Units& units, const MapInfo& map_info, ScriptMode mode);

export struct TriggerVariable {
	std::string name;
	std::string type;
	/// The wtg field after the type. Always 1 in the format notes (wc3libs/War3Net); Needs verification against a stock map.
	uint32_t unknown = 1;
	bool is_array = false;
	int array_size = 0;
	bool is_initialized = false;
	std::string initial_value;
	int id;
	int parent_id;
};

std::string get_base_type(const std::string& type, const ini::INI& trigger_data) {
	std::string base_type = trigger_data.data("TriggerTypes", type, 4);

	if (base_type.empty()) {
		return type;
	}

	return base_type;
}

/// A variable as the Trigger Editor creates it: integer, not an array, no initial value (the World Editor default).
export TriggerVariable make_new_trigger_variable(std::string name, const int id, const int parent_id) {
	TriggerVariable variable;
	variable.name = std::move(name);
	variable.type = "integer";
	variable.id = id;
	variable.parent_id = parent_id;
	return variable;
}

/// Test seam: the udg_ globals and InitGlobals as generate_map_script writes them for these variables.
export std::string generate_variable_script(const std::vector<TriggerVariable>& variables, const ini::INI& trigger_data, ScriptMode mode);

/// Where generate_map_script writes the generated script and how it runs the JASS compiler. The defaults are the production
/// paths; tests point them at a scratch folder and a stub compiler.
export struct MapScriptOutput {
	/// Empty means QDir::tempPath()/input.lua.
	fs::path input_script;
	/// Working directory of the compiler. It writes war3map.j here.
	fs::path tools_directory = "data/tools";
	fs::path compiler = "data/tools/clijasshelper.exe";
	std::chrono::milliseconds compile_timeout = std::chrono::minutes(2);
};

/// Writes `script` to the input file, compiles it (JASS) or syntax-checks it (Lua), and copies the result into the map folder as
/// war3map.lua or war3map.j. On any failure the map's existing script file is left untouched and the error is returned. A Lua syntax
/// error starts with lua_syntax_error_prefix and names the line's entry in `regions` (lines outside them are HiveWE-generated code).
export std::expected<void, std::string> install_map_script(
	std::string_view script,
	ScriptMode mode,
	const MapScriptOutput& output,
	const std::vector<ScriptRegion>& regions = {}
);

export class Triggers {
	std::unordered_map<std::string, int> argument_counts;

	static constexpr int write_version = 0x80000004;
	static constexpr int write_sub_version = 7;
	static constexpr int write_string_version = 1;

	/// One of the eight lists at the start of a 1.31+ wtg: a count of elements of one kind, then the ids of deleted ones.
	/// The kinds are map, library, category, trigger, comment, script, variable and an eighth (1, 1, [0] in human05 and
	/// undead02; 0, 0 in every other stock map). Kept as read so a save writes them back; an import or a new map has zeros.
	/// The counts are not updated on edits, so they lag the live elements after one; older HiveWE wrote zeros, which the
	/// World Editor loads. Whether it minds a stale count: Needs verification. save() is the one place that writes them.
	/// new_element_id() never hands out an id that a deleted list holds.
	struct ElementList {
		uint32_t count = 0;
		std::vector<uint32_t> deleted_ids;
	};
	std::array<ElementList, 8> element_lists;
	/// The u32 after the lists: 2 in all stock maps.
	uint32_t game_version = 2;

	/// Function names a load found no TriggerData entry for. Parsed with no arguments, which is right only if they have none,
	/// so they are named when the load fails.
	std::set<std::string> unknown_function_names;

  public:
	/// Deeper nesting refuses the map, since overflowing the stack cannot be caught. The deepest of the 466
	/// stock maps is 22 (campaign/reforged/roc/undead07).
	static constexpr int max_parse_depth = 256;

	ini::INI trigger_strings;
	ini::INI trigger_data;

	std::string global_jass_comment;
	std::string global_jass;

	std::vector<TriggerCategory> categories;
	std::vector<TriggerVariable> variables;
	std::vector<Trigger> triggers;

	/// Whether any of the wtg's deleted-id lists holds this id.
	bool is_deleted_id(const int id) const {
		const uint32_t stored = static_cast<uint32_t>(id);
		return std::ranges::any_of(element_lists, [&](const ElementList& list) { return std::ranges::contains(list.deleted_ids, stored); });
	}

	/// The id for a new category, trigger, comment, script or variable: the next one after Trigger::next_id that no deleted
	/// list holds. Skipping at allocation rather than raising next_id past every deleted id on load keeps a huge deleted id
	/// (0xFFFFFFFF, or one with high type bits) from pushing next_id toward overflow.
	int new_element_id() const {
		do {
			++Trigger::next_id;
		} while (is_deleted_id(Trigger::next_id));
		return Trigger::next_id;
	}

	/// Removes the first trigger (comment, gui, script), category or variable with this id, as the Trigger Explorer deletes a
	/// tree item. Other kinds are left alone.
	void remove_element(const Classifier kind, const int id) {
		const auto erase_first = [&](auto& elements) {
			if (const auto found = std::ranges::find_if(elements, [&](const auto& element) { return element.id == id; }); found != elements.end()) {
				elements.erase(found);
			}
		};
		switch (kind) {
			case Classifier::comment:
			case Classifier::gui:
			case Classifier::script:
				erase_first(triggers);
				break;
			case Classifier::category:
				erase_first(categories);
				break;
			case Classifier::variable:
				erase_first(variables);
				break;
			default:
				break;
		}
	}

	/// Whether the category `id` is `ancestor_id` or lies below it by the stored parents. Stops at a stored cycle.
	bool is_category_inside(int id, const int ancestor_id) const {
		std::unordered_set<int> visited;
		while (visited.insert(id).second) {
			if (id == ancestor_id) {
				return true;
			}
			const auto found = std::ranges::find_if(categories, [id](const TriggerCategory& category) { return category.id == id; });
			if (found == categories.end()) {
				break;
			}
			id = found->parent_id;
		}
		return false;
	}

	/// Gives the first trigger (comment, gui, script), category or variable with this id a new parent, as the Trigger Explorer
	/// moves a tree item. Other kinds are left alone. A category is never put under itself or its own subcategory: nothing is
	/// written then.
	void set_element_parent(const Classifier kind, const int id, const int parent_id) {
		if (kind == Classifier::category && is_category_inside(parent_id, id)) {
			return;
		}
		const auto reparent_first = [&](auto& elements) {
			if (const auto found = std::ranges::find_if(elements, [&](const auto& element) { return element.id == id; }); found != elements.end()) {
				found->parent_id = parent_id;
			}
		};
		switch (kind) {
			case Classifier::comment:
			case Classifier::gui:
			case Classifier::script:
				reparent_first(triggers);
				break;
			case Classifier::category:
				reparent_first(categories);
				break;
			case Classifier::variable:
				reparent_first(variables);
				break;
			default:
				break;
		}
	}

	/// Lists 1 to 7 all empty: a wtg HiveWE wrote before 2026-11-24 (or a pre-1.31 import or new map not yet saved). The World
	/// Editor writes (1, 0) in the first list in all 466 stock maps, and save() does too when it would otherwise write zeros, so
	/// this tells which war3map.wct layout goes with the file. See load_scripts.
	bool has_legacy_zero_element_lists() const {
		return std::ranges::all_of(element_lists | std::views::take(7), [](const ElementList& list) {
			return list.count == 0 && list.deleted_ids.empty();
		});
	}

	/// Which trigger each war3map.wct entry belongs to, by index into `triggers`. The World Editor writes one entry for every
	/// trigger that is not a comment element (classifier 16), commented-out GUI triggers included, in the order of the trigger
	/// elements in war3map.wtg; save_scripts does the same. HiveWE before 2026-11-24 (legacy) wrote one for each trigger
	/// without is_comment, grouped by category in the order of `categories`: a trigger whose parent is missing got none, and
	/// one under a duplicated category id got one per copy.
	std::vector<std::size_t> custom_text_order(const bool legacy_layout) const {
		std::vector<std::size_t> order;
		if (!legacy_layout) {
			for (std::size_t i = 0; i < triggers.size(); i++) {
				if (triggers[i].classifier != Classifier::comment) {
					order.push_back(i);
				}
			}
			return order;
		}
		for (const TriggerCategory& category : categories) {
			for (std::size_t i = 0; i < triggers.size(); i++) {
				if (triggers[i].parent_id == category.id && !triggers[i].is_comment) {
					order.push_back(i);
				}
			}
		}
		return order;
	}

  private:
	std::string get_type(const std::string_view function_name, size_t parameter) const;

	std::string
	convert_gui_to_jass(const Trigger& trigger, std::vector<std::string>& map_initializations, ScriptMode mode) const;

	std::string resolve_parameter(
		const TriggerParameter& parameter,
		const std::string& trigger_name,
		MapScriptWriter& pre_actions,
		const std::string& type,
		ScriptMode mode,
		bool add_call
	) const;

	std::string convert_eca_to_script(
		const ECA& eca,
		MapScriptWriter& pre_actions,
		const std::string& trigger_name,
		ScriptMode mode,
		bool add_call
	) const;

	static void check_parse_depth(const int depth) {
		if (depth > max_parse_depth) {
			throw std::runtime_error(std::format("Triggers are nested more than {} levels deep", max_parse_depth));
		}
	}

	int argument_count(const std::string& function_name) {
		const auto found = argument_counts.find(function_name);
		if (found == argument_counts.end()) {
			unknown_function_names.insert(function_name);
			return 0;
		}
		return found->second;
	}

	void parse_parameter_structure(BinaryReader& reader, TriggerParameter& parameter, uint32_t version, int depth) {
		check_parse_depth(depth);
		parameter.type = static_cast<TriggerParameter::Type>(reader.read<uint32_t>());
		parameter.value = reader.read_c_string();
		parameter.has_sub_parameter = reader.read<uint32_t>();
		if (parameter.has_sub_parameter) {
			parameter.sub_parameter.type = static_cast<ECA::Type>(reader.read<uint32_t>());
			parameter.sub_parameter.group = 0;
			parameter.sub_parameter.name = reader.read_c_string();
			parameter.sub_parameter.enabled = true;
			const bool has_parameters = reader.read<uint32_t>();
			if (has_parameters) {
				parameter.sub_parameter.parameters.resize(argument_count(parameter.sub_parameter.name));
				for (auto&& i : parameter.sub_parameter.parameters) {
					parse_parameter_structure(reader, i, version, depth + 1);
				}
			}
		}
		if (version == 4) {
			if (parameter.type == TriggerParameter::Type::function) {
				reader.advance(4); // Unknown always 0
			} else {
				parameter.is_array = reader.read<uint32_t>();
			}
		} else {
			if (parameter.has_sub_parameter) {
				parameter.unknown = reader.read<uint32_t>(); // Unknown always 0
			}
			parameter.is_array = reader.read<uint32_t>();
		}
		if (parameter.is_array) {
			parameter.parameters.resize(1);
			parse_parameter_structure(reader, parameter.parameters.front(), version, depth + 1);
		}
	}

	void parse_eca_structure(BinaryReader& reader, ECA& eca, bool is_child, uint32_t version, int depth = 0) {
		check_parse_depth(depth);
		eca.type = static_cast<ECA::Type>(reader.read<uint32_t>());
		if (is_child) {
			eca.group = reader.read<uint32_t>();
		}
		eca.name = reader.read_c_string();
		eca.enabled = reader.read<uint32_t>();
		eca.parameters.resize(argument_count(eca.name));
		for (auto&& i : eca.parameters) {
			parse_parameter_structure(reader, i, version, depth + 1);
		}
		if (version == 7) {
			eca.ecas.resize(reader.read_count(17));
			for (auto&& i : eca.ecas) {
				parse_eca_structure(reader, i, true, version, depth + 1);
			}
		}
	}

	void print_parameter_structure(BinaryWriter& writer, const TriggerParameter& parameter) const {
		writer.write<uint32_t>(static_cast<int>(parameter.type));
		writer.write_c_string(parameter.value);
		writer.write<uint32_t>(parameter.has_sub_parameter);

		if (parameter.has_sub_parameter) {
			writer.write<uint32_t>(static_cast<int>(parameter.sub_parameter.type));
			writer.write_c_string(parameter.sub_parameter.name);
			writer.write<uint32_t>(!parameter.sub_parameter.parameters.empty());
			for (const auto& i : parameter.sub_parameter.parameters) {
				print_parameter_structure(writer, i);
			}

			writer.write<uint32_t>(parameter.unknown);
		}
		writer.write<uint32_t>(parameter.is_array);
		if (parameter.is_array) {
			print_parameter_structure(writer, parameter.parameters.front());
		}
	}

	void print_eca_structure(BinaryWriter& writer, const ECA& eca, bool is_child) const {
		writer.write<uint32_t>(static_cast<int>(eca.type));
		if (is_child) {
			writer.write<uint32_t>(eca.group);
		}

		writer.write_c_string(eca.name);
		writer.write<uint32_t>(eca.enabled);
		for (const auto& i : eca.parameters) {
			print_parameter_structure(writer, i);
		}

		writer.write<uint32_t>(eca.ecas.size());
		for (const auto& i : eca.ecas) {
			print_eca_structure(writer, i, true);
		}
	}

  public:
	void load() {
		BinaryReader reader = hierarchy.map_file_read("war3map.wtg").value();

		trigger_strings.load("UI/TriggerStrings.txt");
		trigger_data.load("UI/TriggerData.txt");
		trigger_data.substitute(world_edit_strings, "WorldEditStrings");

		// Manual fixes
		trigger_data.set_whole_data("TriggerTypeDefaults", "string", "\"\"");

		for (auto&& section : {"TriggerActions"s, "TriggerEvents"s, "TriggerConditions"s, "TriggerCalls"s}) {
			for (const auto& [key, value] : trigger_data.section(section)) {
				if (key.front() == '_') {
					continue;
				}

				int arguments = 0;
				for (const auto& j : value) {
					arguments += !j.empty() && !is_number(j) && j != "nothing";
				}

				if (section == "TriggerCalls") {
					--arguments;
				}

				argument_counts[key] = arguments;
			}
		}

		Trigger::next_id = 0;
		unknown_function_names.clear();
		element_lists = {};
		game_version = 2;

		// Whatever is not understood refuses the map: a map loaded with fewer triggers than it has would lose them on save.
		try {
			const std::string magic_number = reader.read_string(4);
			if (magic_number != "WTG!") {
				throw std::runtime_error("Its magic number is not WTG!");
			}

			const uint32_t version = reader.read<uint32_t>();
			if (version == 0x80000004) {
				load_version_31(reader);
			} else if (version == 4 || version == 7) {
				load_version_pre31(reader, version);
			} else {
				throw std::runtime_error(std::format("Trigger format version {:#x} is not supported", version));
			}

			if (reader.remaining() != 0) {
				throw std::runtime_error(std::format("{} bytes are left after the last trigger element", reader.remaining()));
			}
		} catch (const std::exception& error) {
			if (unknown_function_names.empty()) {
				throw;
			}
			std::string names;
			for (const std::string& name : unknown_function_names) {
				names += (names.empty() ? "" : ", ") + name;
			}
			throw std::runtime_error(std::format("{}. TriggerData has no entry for these trigger functions: {}", error.what(), names));
		}
	}

	void load_version_31(BinaryReader& reader) {
		const uint32_t sub_version = reader.read<uint32_t>();
		// 4 has no sample in any stock map, and HiveWE has always written 7. So the `sub_version == 7` checks below are always
		// true; the format 4 branches they guard are dead and kept only as layout notes.
		if (sub_version != 7) {
			throw std::runtime_error(std::format("Trigger format {} is not supported", sub_version));
		}

		for (ElementList& list : element_lists) {
			list.count = reader.read<uint32_t>();
			list.deleted_ids.resize(reader.read_count(4));
			for (uint32_t& id : list.deleted_ids) {
				id = reader.read<uint32_t>();
			}
		}
		game_version = reader.read<uint32_t>();

		// The smallest variable: two empty names, six ints and an empty initial value.
		const uint32_t variable_count = reader.read_count(27);
		for (uint32_t i = 0; i < variable_count; i++) {
			TriggerVariable variable;
			variable.name = reader.read_c_string();
			variable.type = reader.read_c_string();
			variable.unknown = reader.read<uint32_t>();
			variable.is_array = reader.read<uint32_t>();
			if (sub_version == 7) {
				variable.array_size = reader.read<uint32_t>();
			}
			variable.is_initialized = reader.read<uint32_t>();
			variable.initial_value = reader.read_c_string();
			variable.id = reader.read<uint32_t>();
			variable.parent_id = reader.read<uint32_t>();
			variables.push_back(variable);

			Trigger::next_id = std::max(Trigger::next_id, variable.id + 1);
		}

		// The smallest element: a variable element with an empty name.
		const uint32_t element_count = reader.read_count(13);

		for (uint32_t i = 0; i < element_count; i++) {
			Classifier classifier = static_cast<Classifier>(reader.read<uint32_t>());
			switch (classifier) {
				case Classifier::map:
				case Classifier::library:
				case Classifier::category: {
					TriggerCategory cat;
					cat.classifier = classifier;
					cat.id = reader.read<uint32_t>();
					cat.name = reader.read_c_string();
					if (sub_version == 7) {
						cat.is_comment = reader.read<uint32_t>();
					}
					cat.open_state = reader.read<uint32_t>();
					cat.parent_id = reader.read<uint32_t>();
					categories.push_back(cat);

					Trigger::next_id = std::max(Trigger::next_id, cat.id + 1);
					break;
				}
				case Classifier::gui:
				case Classifier::comment:
				case Classifier::script: {
					Trigger trigger;
					trigger.classifier = classifier;
					trigger.name = reader.read_c_string();
					trigger.description = reader.read_c_string();
					if (sub_version == 7) {
						trigger.is_comment = reader.read<uint32_t>();
					}
					trigger.id = reader.read<uint32_t>();
					trigger.is_enabled = reader.read<uint32_t>();
					trigger.is_script = reader.read<uint32_t>();
					trigger.initially_on = !reader.read<uint32_t>();
					trigger.run_on_initialization = reader.read<uint32_t>();
					trigger.parent_id = reader.read<uint32_t>();
					trigger.ecas.resize(reader.read_count(9));
					for (auto& j : trigger.ecas) {
						parse_eca_structure(reader, j, false, sub_version);
					}

					triggers.push_back(trigger);

					Trigger::next_id = std::max(Trigger::next_id, trigger.id + 1);
					break;
				}
				case Classifier::variable: {
					reader.advance(4); // id
					reader.advance_c_string(); // name
					reader.advance(4); // parentid
					break;
				}
				default:
					throw std::runtime_error(std::format("Trigger element {} has unknown kind {}", i, static_cast<uint32_t>(classifier)));
			}
		}
	}

	void load_version_pre31(BinaryReader& reader, uint32_t version) {
		std::print("Importing pre-1.31 trigger format\n");

		categories.resize(reader.read_count(5));
		for (auto& i : categories) {
			i.classifier = Classifier::category;
			i.id = reader.read<uint32_t>();
			i.name = reader.read_c_string();
			i.parent_id = 0;
			if (version == 7) {
				i.is_comment = reader.read<uint32_t>();
			}

			Trigger::next_id = std::max(Trigger::next_id, i.id + 1);
			if (i.id == 0) {
				i.id = -2;
			}
		}

		reader.advance(4); // dunno

		int variable_category = Trigger::next_id++;
		categories.insert(categories.begin(), {Classifier::map, 0, "Map Header", true, false, -1});
		categories.insert(categories.begin(), {Classifier::category, variable_category, "Variables", true, false, 0});

		variables.resize(reader.read_count(15));
		for (auto& i : variables) {
			i.name = reader.read_c_string();
			i.type = reader.read_c_string();
			i.unknown = reader.read<uint32_t>();
			i.id = Trigger::next_id++;

			i.is_array = reader.read<uint32_t>();
			if (version == 7) {
				i.array_size = reader.read<uint32_t>();
			}
			i.is_initialized = reader.read<uint32_t>();
			i.initial_value = reader.read_c_string();
			i.parent_id = variable_category;
		}

		triggers.resize(reader.read_count(26));
		for (auto& i : triggers) {
			i.name = reader.read_c_string();
			i.description = reader.read_c_string();
			if (version == 7) {
				i.is_comment = reader.read<uint32_t>();
			}
			i.is_enabled = reader.read<uint32_t>();
			i.is_script = reader.read<uint32_t>();
			i.initially_on = !reader.read<uint32_t>();
			i.run_on_initialization = reader.read<uint32_t>();

			i.id = Trigger::next_id++;

			if (i.run_on_initialization && i.is_script) {
				i.classifier = Classifier::gui;
			} else if (i.is_comment) {
				i.classifier = Classifier::comment;
			} else if (i.is_script) {
				i.classifier = Classifier::script;
			} else {
				i.classifier = Classifier::gui;
			}

			i.parent_id = reader.read<uint32_t>();
			if (i.parent_id == 0) {
				i.parent_id = -2;
			}
			i.ecas.resize(reader.read_count(9));
			for (auto& j : i.ecas) {
				parse_eca_structure(reader, j, false, version);
			}
		}
	}

	/// Can be Jass or Lua depending on the map
	void load_scripts() {
		BinaryReader reader = hierarchy.map_file_read("war3map.wct").value();

		const uint32_t version = reader.read<uint32_t>();
		if (version != 0x80000004) {
			if (version == 1 || version == 0) {
				if (version == 1) {
					global_jass_comment = reader.read_c_string();
					global_jass = reader.read_string(reader.read<uint32_t>());
				}
				reader.advance(4);
				for (auto&& i : triggers) {
					const uint32_t size = reader.read<uint32_t>();
					if (size > 0) {
						i.custom_text = reader.read_string(size);
					}
				}
				check_wct_rest_is_zero(reader);
				return;
			} else {
				std::print("Probably invalid WCT format\n");
			}
		}

		const int sub_version = reader.read<uint32_t>();
		if (sub_version != 1 && sub_version != 0) {
			std::print("Unknown WCT 1.31 subformat\n");
		}

		if (sub_version == 1) {
			global_jass_comment = reader.read_c_string();
			const uint32_t size = reader.read<uint32_t>();
			if (size > 0) {
				global_jass = reader.read_string(size);
			}
		}

		// Which trigger each entry belongs to depends on the writer (custom_text_order). The wtg's element lists say which wrote
		// it. Only a file marked as older HiveWE's may fall back to the World Editor layout. An unmarked file gets no fallback:
		// any layout with fewer entries would accept a World Editor wct cut short and silently drop or shift its text.
		const bool legacy_layout = has_legacy_zero_element_lists();
		std::optional<std::vector<std::string>> texts = read_custom_texts(reader, legacy_layout);
		if (!texts && legacy_layout) {
			texts = read_custom_texts(reader, false);
		}
		if (!texts) {
			throw std::runtime_error("Its custom text entries do not match the triggers in war3map.wtg");
		}
		for (std::size_t i = 0; i < triggers.size(); i++) {
			triggers[i].custom_text = std::move((*texts)[i]);
		}
	}

	/// The custom text of every trigger, by index, if the entries of this layout read to the end of the file with only zeros
	/// after them. On failure the reader is back where it started.
	std::optional<std::vector<std::string>> read_custom_texts(BinaryReader& reader, const bool legacy_layout) const {
		const auto start = reader.position;
		std::vector<std::string> texts(triggers.size());
		try {
			for (const std::size_t i : custom_text_order(legacy_layout)) {
				const uint32_t size = reader.read<uint32_t>();
				// A trigger listed twice (duplicate category id) got the same text both times.
				texts[i] = size > 0 ? reader.read_string(size) : std::string();
			}
			check_wct_rest_is_zero(reader);
		} catch (const std::exception&) {
			reader.position = start;
			return std::nullopt;
		}
		return texts;
	}

	/// Trailing zeros are common in stock wct files and harmless. Anything else is custom text no trigger took.
	static void check_wct_rest_is_zero(BinaryReader& reader) {
		const long long remaining = reader.remaining();
		for (long long i = 0; i < remaining; i++) {
			if (reader.read<uint8_t>() != 0) {
				throw std::runtime_error(std::format("{} bytes are left after the last custom text, and they are not all zero", remaining));
			}
		}
	}

	void save() const {
		BinaryWriter writer;
		writer.write_string("WTG!");
		writer.write<uint32_t>(write_version);
		writer.write<uint32_t>(write_sub_version);

		// Written as read (see ElementList), except that lists 1 to 7 are never all empty: that marks a wct in the layout HiveWE
		// wrote before 2026-11-24, and save_scripts writes the World Editor's. (1, 0) in the first list is what the World Editor
		// writes in all 466 stock maps.
		std::array<ElementList, 8> lists = element_lists;
		if (has_legacy_zero_element_lists()) {
			lists[0].count = 1;
		}
		for (const ElementList& list : lists) {
			writer.write<uint32_t>(list.count);
			writer.write<uint32_t>(list.deleted_ids.size());
			for (const uint32_t id : list.deleted_ids) {
				writer.write<uint32_t>(id);
			}
		}
		writer.write<uint32_t>(game_version);
		writer.write<uint32_t>(variables.size());

		for (const auto& i : variables) {
			writer.write_c_string(i.name);
			writer.write_c_string(i.type);
			writer.write<uint32_t>(i.unknown);
			writer.write<uint32_t>(i.is_array);
			writer.write<uint32_t>(i.array_size);
			writer.write<uint32_t>(i.is_initialized);
			writer.write_c_string(i.initial_value);
			writer.write<uint32_t>(i.id);
			writer.write<uint32_t>(i.parent_id);
		}

		writer.write<uint32_t>(categories.size() + triggers.size() + variables.size());

		for (const auto& i : categories) {
			writer.write<uint32_t>(static_cast<int>(i.classifier));
			writer.write<uint32_t>(i.id);
			writer.write_c_string(i.name);
			writer.write<uint32_t>(i.is_comment);
			writer.write<uint32_t>(i.open_state);
			writer.write<uint32_t>(i.parent_id);
		}

		for (const auto& i : triggers) {
			writer.write<uint32_t>(static_cast<int>(i.classifier));
			writer.write_c_string(i.name);
			writer.write_c_string(i.description);

			writer.write<uint32_t>(i.is_comment);
			writer.write<uint32_t>(i.id);
			writer.write<uint32_t>(i.is_enabled);
			writer.write<uint32_t>(i.is_script);
			writer.write<uint32_t>(!i.initially_on);
			writer.write<uint32_t>(i.run_on_initialization);
			writer.write<uint32_t>(i.parent_id);
			writer.write<uint32_t>(i.ecas.size());
			for (const auto& eca : i.ecas) {
				print_eca_structure(writer, eca, false);
			}
		}

		for (const auto& i : variables) {
			writer.write<uint32_t>(static_cast<int>(Classifier::variable));
			writer.write<uint32_t>(i.id);
			writer.write_c_string(i.name);
			writer.write<uint32_t>(i.parent_id);
		}

		hierarchy.map_file_write("war3map.wtg", writer.buffer);
	}

	/// Can be jass or lua depending on the map
	void save_scripts() const {
		BinaryWriter writer;

		writer.write<uint32_t>(write_version);
		writer.write<uint32_t>(1);

		writer.write_c_string(global_jass_comment);
		if (global_jass.empty()) {
			writer.write<uint32_t>(0);
		} else {
			writer.write<uint32_t>(global_jass.size() + (global_jass.back() == '\0' ? 0 : 1));
			writer.write_c_string(global_jass);
		}

		// One entry per trigger in the order save() writes the trigger elements, which is the order load_scripts reads them in.
		// Grouping by category here once moved text between triggers and wrote triggers of duplicate category ids twice.
		for (const std::size_t index : custom_text_order(false)) {
			const Trigger& i = triggers[index];
			if (i.custom_text.empty()) {
				writer.write<uint32_t>(0);
			} else {
				writer.write<uint32_t>(i.custom_text.size() + (i.custom_text.back() == '\0' ? 0 : 1));
				writer.write_c_string(i.custom_text);
			}
		}

		hierarchy.map_file_write("war3map.wct", writer.buffer);
	}

	/// Returns compile output which could contain errors or general information
	[[nodiscard]] std::expected<void, std::string> generate_map_script(
		const Terrain& terrain,
		const Units& units,
		const Doodads& doodads,
		const MapInfo& map_info,
		const Sounds& sounds,
		const Regions& regions,
		const GameCameras& cameras,
		ScriptMode mode
	) const;
};