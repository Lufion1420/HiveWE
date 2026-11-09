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
