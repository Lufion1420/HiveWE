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

TEST_CASE("Lua custom-text triggers get no gg_trg_ global or InitTrig_ call, JASS ones do") {
	Trigger gui;
	Trigger lua_library;
	lua_library.custom_text = "Debug.beginFile(\"x\")";
	Trigger disabled;
	disabled.is_enabled = false;
	Trigger comment;
	comment.is_comment = true;

	CHECK(emits_trigger_identifier(gui, ScriptMode::lua));
	CHECK(emits_trigger_identifier(gui, ScriptMode::jass));
	CHECK_FALSE(emits_trigger_identifier(lua_library, ScriptMode::lua));
	CHECK(emits_trigger_identifier(lua_library, ScriptMode::jass));
	CHECK_FALSE(emits_trigger_identifier(disabled, ScriptMode::lua));
	CHECK_FALSE(emits_trigger_identifier(comment, ScriptMode::jass));
}

TEST_CASE("prefixed identifiers trim before sanitizing, so global and use agree") {
	CHECK(prefixed_script_identifier("gg_rct_", "  My Region ") == "gg_rct_My_Region");
	CHECK(prefixed_script_identifier("InitTrig_", "Kelen's Dagger") == "InitTrig_Kelen_s_Dagger");
	CHECK(prefixed_script_identifier("", "gg_snd_A B") == "gg_snd_A_B");
}
