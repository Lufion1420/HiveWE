#include <doctest/doctest.h>

import std;
import <glm/glm.hpp>;
import Globals;
import Terrain;
import Triggers;
import Units;

namespace {

	const Terrain& test_terrain() {
		static const Terrain* terrain = new Terrain(); // Never destroyed: its destructor calls GL.
		return *terrain;
	}

	bool contains(const std::string& haystack, const std::string_view needle) {
		return haystack.find(needle) != std::string::npos;
	}

} // namespace

TEST_CASE("autocast ability orders of placed units are written as string literals") {
	// ctest runs each case in its own process, so the global abilities table only holds these test rows.
	abilities_slk.set_shadow_data("orderon", "Abtl", "bloodluston");
	abilities_slk.set_shadow_data("orderoff", "Abtl", "bloodlustoff");
	abilities_slk.set_shadow_data("order", "Aord", "slow");

	Units units;
	Unit unit;
	unit.id = "hfoo";
	unit.skin_id = "hfoo";
	unit.position = glm::vec3(1.f, 2.f, 0.f);
	unit.abilities.emplace_back("Abtl", 0, 0); // Autocast off: orderoff
	unit.abilities.emplace_back("Abtl", 1, 0); // Autocast on: orderon
	unit.abilities.emplace_back("Aord", 1, 0); // Autocast on without orderon: order
	units.units.push_back(unit);

	const std::string lua = generate_unit_script(test_terrain(), units, ScriptMode::lua);
	CHECK(contains(lua, "\tIssueImmediateOrder(u, \"bloodlustoff\")\n"));
	CHECK(contains(lua, "\tIssueImmediateOrder(u, \"bloodluston\")\n"));
	CHECK(contains(lua, "\tIssueImmediateOrder(u, \"slow\")\n"));
	CHECK_FALSE(contains(lua, "['"));

	const std::string jass = generate_unit_script(test_terrain(), units, ScriptMode::jass);
	CHECK(contains(jass, "\tcall IssueImmediateOrder(u, \"bloodlustoff\")\n"));
	CHECK(contains(jass, "\tcall IssueImmediateOrder(u, \"bloodluston\")\n"));
	CHECK(contains(jass, "\tcall IssueImmediateOrder(u, \"slow\")\n"));
	CHECK_FALSE(contains(jass, "['"));
}
