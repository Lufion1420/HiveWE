#include <doctest/doctest.h>

import std;
import INI;
import Triggers;

namespace {
	ini::INI make_trigger_data() {
		ini::INI data;
		data.set_whole_data("TriggerTypes", "integer", "1,1,1,Integer,integer");
		data.set_whole_data("TriggerTypeDefaults", "integer", "0");
		return data;
	}

	bool contains(const std::string& haystack, const std::string_view needle) {
		return haystack.find(needle) != std::string::npos;
	}
} // namespace

TEST_CASE("a new trigger variable is an integer with defined flags") {
	const TriggerVariable variable = make_new_trigger_variable("NewVariable", 7, 3);

	CHECK(variable.name == "NewVariable");
	CHECK(variable.type == "integer");
	CHECK(variable.unknown == 1);
	CHECK_FALSE(variable.is_array);
	CHECK_FALSE(variable.is_initialized);
	CHECK(variable.array_size == 0);
	CHECK(variable.initial_value.empty());
	CHECK(variable.id == 7);
	CHECK(variable.parent_id == 3);
}

TEST_CASE("a default-constructed trigger variable has no garbage flags") {
	TriggerVariable variable;
	CHECK(variable.unknown == 1);
	CHECK_FALSE(variable.is_array);
	CHECK_FALSE(variable.is_initialized);
}

TEST_CASE("a new variable is declared and initialised as an integer global") {
	const std::vector<TriggerVariable> variables = { make_new_trigger_variable("NewVariable", 1, 0) };
	const ini::INI trigger_data = make_trigger_data();

	const std::string lua = generate_variable_script(variables, trigger_data, ScriptMode::lua);
	CHECK(contains(lua, "udg_NewVariable = 0\n"));
	CHECK(contains(lua, "function InitGlobals()"));

	const std::string jass = generate_variable_script(variables, trigger_data, ScriptMode::jass);
	CHECK(contains(jass, "integer udg_NewVariable = 0\n"));
	CHECK(contains(jass, "set udg_NewVariable = 0\n"));
}
