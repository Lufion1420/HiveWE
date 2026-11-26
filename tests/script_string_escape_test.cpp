#include <doctest/doctest.h>

import std;
import <glm/glm.hpp>;
import Globals;
import INI;
import LuaSyntax;
import Sounds;
import Terrain;
import Triggers;
import Units;
import Utilities;
import AssetObfuscation;

// Map-derived text the script generator puts inside a double-quoted script literal: backslash, quote and line feed are escaped,
// a carriage return is dropped, everything else (tabs, UTF-8) passes through. Code (custom script, comments) stays raw.

namespace {

	const Terrain& test_terrain() {
		static const Terrain* terrain = new Terrain(); // Never destroyed: its destructor calls GL.
		return *terrain;
	}

	bool contains(const std::string& haystack, const std::string_view needle) {
		return haystack.find(needle) != std::string::npos;
	}

	bool parses_as_lua(const std::string& script) {
		const std::optional<LuaSyntaxError> error = check_lua_syntax(script);
		if (error) {
			MESSAGE("Lua syntax error on line ", error->line, ": ", error->message, "\n", script);
		}
		return !error;
	}

	Sound plain_sound(const std::string& name) {
		Sound sound {};
		sound.name = name;
		sound.eax_effect = "DefaultEAXON";
		sound.volume = 100;
		sound.channel = -1;
		return sound;
	}

	ini::INI string_trigger_data() {
		ini::INI data;
		data.ini_data["TriggerTypes"]["string"] = { "0", "1", "1", "String", "string" };
		data.ini_data["TriggerTypeDefaults"]["string"] = { "\"\"" };
		data.ini_data["TriggerParams"]["unused"] = { "" };
		return data;
	}

	TriggerVariable string_variable(const std::string& name, const std::string& initial_value, const bool is_array = false) {
		TriggerVariable variable = make_new_trigger_variable(name, 1, 0);
		variable.type = "string";
		variable.is_initialized = true;
		variable.initial_value = initial_value;
		variable.is_array = is_array;
		variable.array_size = is_array ? 2 : 0;
		return variable;
	}

	TriggerParameter parameter(const TriggerParameter::Type type, const std::string& value) {
		TriggerParameter result {};
		result.type = type;
		result.value = value;
		return result;
	}

	ECA action(const std::string& name, const std::vector<TriggerParameter>& parameters) {
		ECA result {};
		result.type = ECA::Type::action;
		result.name = name;
		result.enabled = true;
		result.parameters = parameters;
		return result;
	}

	/// A GUI trigger "QA" with a string action, a custom script line and a comment.
	Trigger gui_trigger(const std::string& text, const std::string& custom_script, const std::string& comment) {
		Trigger trigger = make_new_trigger(Classifier::gui, "QA", 1, 0);
		trigger.ecas.push_back(action("QA_Print", { parameter(TriggerParameter::Type::string, text) }));
		trigger.ecas.push_back(action("CustomScriptCode", { parameter(TriggerParameter::Type::string, custom_script) }));
		trigger.ecas.push_back(action("CommentString", { parameter(TriggerParameter::Type::string, comment) }));
		return trigger;
	}

	std::string gui_script(const Trigger& trigger, const ScriptMode mode) {
		Triggers triggers;
		triggers.trigger_data = string_trigger_data();
		triggers.trigger_data.ini_data["TriggerActions"]["QA_Print"] = { "0", "string" };
		return triggers.generate_gui_trigger_script(trigger, mode);
	}

} // namespace

TEST_CASE("script_string_literal escapes backslash, quote and line feed and drops carriage returns") {
	CHECK(script_string_literal("") == "\"\"");
	CHECK(script_string_literal("war3mapImported\\x.mdx") == "\"war3mapImported\\\\x.mdx\"");
	CHECK(script_string_literal("say \"hi\"") == "\"say \\\"hi\\\"\"");
	CHECK(script_string_literal("a\nb") == "\"a\\nb\"");
	CHECK(script_string_literal("a\r\nb\rc") == "\"a\\nbc\"");
	CHECK(script_string_literal("a\tb\x01") == "\"a\tb\x01\"");
	CHECK(script_string_literal("\xC3\xA4\xE2\x9C\x93") == "\"\xC3\xA4\xE2\x9C\x93\"");
	CHECK(script_string_literal("TRIGSTR_007") == "\"TRIGSTR_007\"");
	CHECK(script_string_literal("|cffff0000Red|r|n") == "\"|cffff0000Red|r|n\"");
}

TEST_CASE("Map Protector's escape_script_string keeps dropping control bytes") {
	CHECK(escape_script_string("a\tb\x01\"\\\r\nc\xC3\xA4") == "ab\\\"\\\\\\nc\xC3\xA4");
	CHECK(escape_script_string("war3mapImported\\x.mdx") == "war3mapImported\\\\x.mdx");
}

TEST_CASE("sound strings with quotes and line breaks are escaped in InitSounds") {
	Sounds sounds;
	Sound sound = plain_sound("gg_snd_Line");
	sound.file = "war3mapImported\\Line \"1\".wav";
	sound.label = "Say \"hi\"\nnow";
	sound.facial_animation_label = "Face\r\nTwo";
	sound.facial_animation_group_label = "Map\tTab";
	sound.facial_animation_set_path = "Faces\\\"x\".animset";
	sounds.sounds.push_back(sound);
	const auto duration = [](const Sound&) { return std::optional<int>(100); };

	const std::string lua = generate_sound_script(sounds, ScriptMode::lua, duration);
	CHECK(lua
		  == "function InitSounds()\n"
			 "\tgg_snd_Line = CreateSound(\"war3mapImported\\\\Line \\\"1\\\".wav\", false, false, false, 0, 0, \"DefaultEAXON\")\n"
			 "\tSetSoundParamsFromLabel(gg_snd_Line, \"Say \\\"hi\\\"\\nnow\")\n"
			 "\tSetSoundFacialAnimationLabel(gg_snd_Line, \"Face\\nTwo\")\n"
			 "\tSetSoundFacialAnimationGroupLabel(gg_snd_Line, \"Map\tTab\")\n"
			 "\tSetSoundFacialAnimationSetFilepath(gg_snd_Line, \"Faces\\\\\\\"x\\\".animset\")\n"
			 "\tSetSoundDuration(gg_snd_Line, 100)\n"
			 "\tSetSoundVolume(gg_snd_Line, 100)\n"
			 "end\n\n");
	CHECK(parses_as_lua(lua));

	const std::string jass = generate_sound_script(sounds, ScriptMode::jass, duration);
	CHECK(contains(jass, "\tcall SetSoundParamsFromLabel(gg_snd_Line, \"Say \\\"hi\\\"\\nnow\")\n"));
	CHECK(contains(jass, "\tcall SetSoundFacialAnimationSetFilepath(gg_snd_Line, \"Faces\\\\\\\"x\\\".animset\")\n"));
}

TEST_CASE("sound strings with nothing to escape are written as before") {
	Sounds sounds;
	Sound sound = plain_sound("gg_snd_Music");
	sound.music = true;
	sound.file = "war3mapImported\\Music.mp3;Sound\\Music\\mp3Music\\Comradeship.mp3";
	sounds.sounds.push_back(sound);
	Sound effect = plain_sound("gg_snd_Ping");
	effect.file = "Sound\\Interface\\Ping.wav";
	effect.label = "PingLabel";
	effect.speaker_key = 7;
	sounds.sounds.push_back(effect);
	const auto duration = [](const Sound&) { return std::optional<int>(250); };

	// Captured from the build before the shared escaping helper.
	CHECK(generate_sound_script(sounds, ScriptMode::lua, duration)
		  == "function InitSounds()\n"
			 "\tgg_snd_Music = \"war3mapImported\\\\Music.mp3;Sound\\\\Music\\\\mp3Music\\\\Comradeship.mp3\"\n"
			 "\tgg_snd_Ping = CreateSound(\"Sound\\\\Interface\\\\Ping.wav\", false, false, false, 0, 0, \"DefaultEAXON\")\n"
			 "\tSetSoundParamsFromLabel(gg_snd_Ping, \"PingLabel\")\n"
			 "\tSetDialogueSpeakerNameKey(gg_snd_Ping, \"TRIGSTR_7\")\n"
			 "\tSetSoundDuration(gg_snd_Ping, 250)\n"
			 "\tSetSoundVolume(gg_snd_Ping, 100)\n"
			 "end\n\n");
}

TEST_CASE("string variable initial values are escaped, presets and plain text are not changed") {
	const ini::INI trigger_data = string_trigger_data();
	const std::vector<TriggerVariable> variables = {
		string_variable("Text", "He said \"go\"\r\nTab\there \xC3\xA4"),
		string_variable("Lines", "a\nb", true),
		string_variable("Key", "TRIGSTR_007"),
		string_variable("Path", "war3mapImported\\x.blp"),
	};

	const std::string lua = generate_variable_script(variables, trigger_data, ScriptMode::lua);
	CHECK(contains(lua, "\tudg_Text = \"He said \\\"go\\\"\\nTab\there \xC3\xA4\"\n"));
	CHECK(contains(lua, "\t\tudg_Lines[i] = \"a\\nb\"\n"));
	CHECK(contains(lua, "\tudg_Key = \"TRIGSTR_007\"\n"));
	CHECK(contains(lua, "\tudg_Path = \"war3mapImported\\\\x.blp\"\n"));
	CHECK(parses_as_lua(lua));

	const std::string jass = generate_variable_script(variables, trigger_data, ScriptMode::jass);
	CHECK(contains(jass, "\tset udg_Text = \"He said \\\"go\\\"\\nTab\there \xC3\xA4\"\n"));
	CHECK(contains(jass, "\tset udg_Key = \"TRIGSTR_007\"\n"));
	CHECK(contains(jass, "\tset udg_Path = \"war3mapImported\\\\x.blp\"\n"));
}

TEST_CASE("ability order strings of placed units are escaped") {
	// ctest runs each case in its own process, so the global abilities table only holds these test rows.
	abilities_slk.set_shadow_data("orderon", "Aqq1", "say\"x\"\nnow");
	abilities_slk.set_shadow_data("orderon", "Aqq2", "slow");

	Units units;
	Unit unit;
	unit.id = "hfoo";
	unit.skin_id = "hfoo";
	unit.position = glm::vec3(1.f, 2.f, 0.f);
	unit.abilities.emplace_back("Aqq1", 1, 0);
	unit.abilities.emplace_back("Aqq2", 1, 0);
	units.units.push_back(unit);

	const std::string lua = generate_unit_script(test_terrain(), units, ScriptMode::lua);
	CHECK(contains(lua, "\tIssueImmediateOrder(u, \"say\\\"x\\\"\\nnow\")\n"));
	CHECK(contains(lua, "\tIssueImmediateOrder(u, \"slow\")\n"));
	CHECK(parses_as_lua(lua));

	const std::string jass = generate_unit_script(test_terrain(), units, ScriptMode::jass);
	CHECK(contains(jass, "\tcall IssueImmediateOrder(u, \"say\\\"x\\\"\\nnow\")\n"));
}

TEST_CASE("object ids in Lua FourCC calls are escaped") {
	Units units;
	Unit unit;
	unit.id = "h\"o\\";
	unit.skin_id = "hfoo";
	unit.position = glm::vec3(1.f, 2.f, 0.f);
	units.units.push_back(unit);

	const std::string lua = generate_unit_script(test_terrain(), units, ScriptMode::lua);
	CHECK(contains(lua, "FourCC(\"h\\\"o\\\\\")"));
	CHECK(contains(lua, "FourCC(\"hfoo\")"));
	CHECK(parses_as_lua(lua));
}

TEST_CASE("GUI string parameters are escaped once, custom script and comments stay raw") {
	const Trigger trigger = gui_trigger("Line \"one\"\nC:\\Path \xE2\x9C\x93", "call BJDebugMsg(\"a\\nb\")", "note \"quoted\" C:\\x");

	const std::string lua = gui_script(trigger, ScriptMode::lua);
	CHECK(contains(lua, "\nQA_Print(\"Line \\\"one\\\"\\nC:\\\\Path \xE2\x9C\x93\")\n"));
	CHECK(contains(lua, "\ncall BJDebugMsg(\"a\\nb\")\n"));
	CHECK(contains(lua, "\n--note \"quoted\" C:\\x\n"));

	const Trigger lua_trigger = gui_trigger("Line \"one\"\nC:\\Path", "BJDebugMsg(\"a\\nb\")", "note");
	CHECK(parses_as_lua(gui_script(lua_trigger, ScriptMode::lua)));

	const std::string jass = gui_script(trigger, ScriptMode::jass);
	CHECK(contains(jass, "\ncall QA_Print(\"Line \\\"one\\\"\\nC:\\\\Path \xE2\x9C\x93\")\n"));
	CHECK(contains(jass, "\ncall BJDebugMsg(\"a\\nb\")\n"));
	CHECK(contains(jass, "\n//note \"quoted\" C:\\x\n"));
}

TEST_CASE("GUI string parameters with nothing to escape are written as before") {
	const Trigger trigger = gui_trigger("TRIGSTR_007", "set udg_X = 1", "plain");
	// Captured from the build before the shared escaping helper.
	CHECK(gui_script(trigger, ScriptMode::lua)
		  == "function Trig_QA_Actions()\n"
			 "QA_Print(\"TRIGSTR_007\")\n"
			 "set udg_X = 1\n"
			 "--plain\n"
			 "end\n\n"
			 "function InitTrig_QA()\n"
			 "\tgg_trg_QA = CreateTrigger()\n"
			 "\tTriggerAddAction(gg_trg_QA, Trig_QA_Actions)\n"
			 "end\n\n");
}
