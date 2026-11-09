#include <doctest/doctest.h>

import std;
import Triggers;

TEST_CASE("operator presets lose the TriggerData quotes in both script modes") {
	// TriggerData: OperatorNotEqualENE=0,EqualNotEqualOperator,"!=",...
	CHECK(normalize_preset_value("EqualNotEqualOperator", "\"!=\"", ScriptMode::jass) == "!=");
	CHECK(normalize_preset_value("EqualNotEqualOperator", "\"!=\"", ScriptMode::lua) == "~=");
	CHECK(normalize_preset_value("EqualNotEqualOperator", "\"==\"", ScriptMode::jass) == "==");
	CHECK(normalize_preset_value("EqualNotEqualOperator", "\"==\"", ScriptMode::lua) == "==");
	CHECK(normalize_preset_value("ComparisonOperator", "\"<=\"", ScriptMode::lua) == "<=");
	CHECK(normalize_preset_value("ComparisonOperator", "\">=\"", ScriptMode::jass) == ">=");
	CHECK(normalize_preset_value("ArithmeticOperator", "\"+\"", ScriptMode::lua) == "+");
}

TEST_CASE("null presets become nil only in Lua") {
	CHECK(normalize_preset_value("unit", "null", ScriptMode::lua) == "nil");
	CHECK(normalize_preset_value("unit", "null", ScriptMode::jass) == "null");
}

TEST_CASE("other preset values are untouched") {
	CHECK(normalize_preset_value("string", "\"hello\"", ScriptMode::lua) == "\"hello\"");
	CHECK(normalize_preset_value("integer", "5", ScriptMode::jass) == "5");
}

TEST_CASE("script identifiers replace every character outside [A-Za-z0-9_]") {
	CHECK(script_identifier("Kelen's Dagger of Escape") == "Kelen_s_Dagger_of_Escape");
	CHECK(script_identifier("========= QUEST API DOCUMENTATION ============") == "__________QUEST_API_DOCUMENTATION_____________");
	CHECK(script_identifier("Plain_Name1") == "Plain_Name1");
	CHECK(script_identifier("gg_trg_A-B") == "gg_trg_A_B");
}

TEST_CASE("an event without parameters has no trailing argument separator") {
	CHECK(event_call_arguments("gg_trg_TS_Main_Init", "") == "gg_trg_TS_Main_Init");
	CHECK(event_call_arguments("gg_trg_A", "Player(0), 5") == "gg_trg_A, Player(0), 5");
}

TEST_CASE("Lua script elements get no gg_trg_ global or InitTrig_ call, JASS ones and converted GUI triggers do") {
	Trigger gui;
	gui.classifier = Classifier::gui;
	Trigger lua_library; // 0239 "Load Order": classifier 32
	lua_library.classifier = Classifier::script;
	lua_library.is_script = true;
	lua_library.custom_text = "Debug.beginFile(\"x\")";
	Trigger empty_library; // 0239 "Cursed Flute": classifier 32 with no text
	empty_library.classifier = Classifier::script;
	empty_library.is_script = true;
	Trigger converted_gui; // 0240 "BeginMap": classifier 8 converted to custom text
	converted_gui.classifier = Classifier::gui;
	converted_gui.is_script = true;
	converted_gui.custom_text = "function InitTrig_BeginMap()\nend";
	Trigger disabled;
	disabled.classifier = Classifier::gui;
	disabled.is_enabled = false;
	Trigger comment;
	comment.classifier = Classifier::comment;
	comment.is_comment = true;

	CHECK(emits_trigger_identifier(gui, ScriptMode::lua));
	CHECK(emits_trigger_identifier(gui, ScriptMode::jass));
	CHECK_FALSE(emits_trigger_identifier(lua_library, ScriptMode::lua));
	CHECK(emits_trigger_identifier(lua_library, ScriptMode::jass));
	CHECK_FALSE(emits_trigger_identifier(empty_library, ScriptMode::lua));
	CHECK(emits_trigger_identifier(empty_library, ScriptMode::jass));
	CHECK(emits_trigger_identifier(converted_gui, ScriptMode::lua));
	CHECK(emits_trigger_identifier(converted_gui, ScriptMode::jass));
	CHECK_FALSE(emits_trigger_identifier(disabled, ScriptMode::lua));
	CHECK_FALSE(emits_trigger_identifier(comment, ScriptMode::jass));
}

TEST_CASE("a Lua GUI trigger converted to custom text is initialised and run on map init like the World Editor does") {
	Trigger begin_map; // 0240 Silverpine Sprint "BeginMap"
	begin_map.classifier = Classifier::gui;
	begin_map.name = "BeginMap";
	begin_map.is_script = true;
	begin_map.run_on_initialization = true;
	begin_map.custom_text = "function InitTrig_BeginMap()\ngg_trg_BeginMap = CreateTrigger()\nend";
	Trigger library; // Script element: plain code, never initialised
	library.classifier = Classifier::script;
	library.name = "Load Order";
	library.is_script = true;
	library.run_on_initialization = true;
	library.custom_text = "do end";
	const std::vector<Trigger> triggers = {begin_map, library};

	std::string lua_script;
	std::vector<std::string> lua_init;
	for (const auto& trigger : triggers) {
		append_custom_text_trigger(lua_script, lua_init, trigger, ScriptMode::lua);
	}
	CHECK(lua_script.starts_with("function InitTrig_BeginMap()\nend\n\nfunction InitTrig_BeginMap()\ngg_trg_BeginMap"));
	CHECK(lua_script.find("InitTrig_Load_Order") == std::string::npos);
	CHECK(lua_init == std::vector<std::string> {"gg_trg_BeginMap"});
	const std::string lua = generate_trigger_initialization_script(lua_init, triggers, ScriptMode::lua);
	CHECK(lua.find("\tInitTrig_BeginMap()\n") != std::string::npos);
	CHECK(lua.find("\tConditionalTriggerExecute(gg_trg_BeginMap)\n") != std::string::npos);
	CHECK(lua.find("Load_Order") == std::string::npos);

	// JASS: no empty InitTrig_ (it would be a duplicate function), every enabled trigger initialised.
	std::string jass_script;
	std::vector<std::string> jass_init;
	for (const auto& trigger : triggers) {
		append_custom_text_trigger(jass_script, jass_init, trigger, ScriptMode::jass);
	}
	CHECK(jass_script == begin_map.custom_text + "\n" + library.custom_text + "\n");
	CHECK(jass_init == std::vector<std::string> {"gg_trg_BeginMap", "gg_trg_Load_Order"});
	const std::string jass = generate_trigger_initialization_script(jass_init, triggers, ScriptMode::jass);
	CHECK(jass.find("\tcall InitTrig_BeginMap()\n") != std::string::npos);
	CHECK(jass.find("\tcall InitTrig_Load_Order()\n") != std::string::npos);
	CHECK(jass.find("\tcall ConditionalTriggerExecute(gg_trg_BeginMap)\n") != std::string::npos);
}

TEST_CASE("prefixed identifiers trim before sanitizing, so global and use agree") {
	CHECK(prefixed_script_identifier("gg_rct_", "  My Region ") == "gg_rct_My_Region");
	CHECK(prefixed_script_identifier("InitTrig_", "Kelen's Dagger") == "InitTrig_Kelen_s_Dagger");
	CHECK(prefixed_script_identifier("", "gg_snd_A B") == "gg_snd_A_B");
}

TEST_CASE("destructable creation numbers are padded the same in lookup and name") {
	CHECK(padded_creation_number(144) == "0144");
	CHECK(padded_creation_number(1220) == "1220");
	CHECK(padded_creation_number(12345) == "12345");
	CHECK(destructable_global_name("B000", 144) == "gg_dest_B000_0144");
	CHECK(destructable_global_name("B000", 12345) == "gg_dest_B000_12345");
	// The trigger text holds "gg_dest_B000_0144"; the parsed key must equal the lookup key.
	const std::string text = "gg_dest_B000_0144";
	CHECK(text.substr(13) == padded_creation_number(144));
}

TEST_CASE("Lua global defaults match the World Editor") {
	CHECK(lua_global_default("integer", false) == "0");
	CHECK(lua_global_default("real", false) == "0.0");
	CHECK(lua_global_default("boolean", false) == "false");
	CHECK(lua_global_default("string", false) == "\"\"");
	CHECK(lua_global_default("unit", false) == "nil");
	CHECK(lua_global_default("integer", true) == "__jarray(0)");
	CHECK(lua_global_default("real", true) == "__jarray(0.0)");
	CHECK(lua_global_default("boolean", true) == "__jarray(false)");
	CHECK(lua_global_default("string", true) == "__jarray(\"\")");
	CHECK(lua_global_default("unit", true) == "{}");
}
