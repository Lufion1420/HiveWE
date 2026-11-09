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

namespace {

	Unit placed_unit(const std::string& id) {
		Unit unit;
		unit.id = id;
		unit.skin_id = id;
		unit.position = glm::vec3(1.f, 2.f, 0.f);
		return unit;
	}

	std::string script_for(const Unit& unit, const ScriptMode mode) {
		Units units;
		units.units.push_back(unit);
		return generate_unit_script(test_terrain(), units, mode);
	}

	void set_colour_test_rows() {
		// ctest runs each case in its own process, so the global units table only holds these test rows.
		units_slk.set_shadow_data("teamcolor", "ntav", "0");
		units_slk.set_shadow_data("customteamcolor", "ntav", "0");
		units_slk.set_shadow_data("teamcolor", "nwgt", "-1");
		units_slk.set_shadow_data("customteamcolor", "nwgt", "1");
		units_slk.set_shadow_data("teamcolor", "hfoo", "-1");
		units_slk.set_shadow_data("customteamcolor", "hfoo", "0");
	}

} // namespace

TEST_CASE("placed units get SetUnitColor from teamcolor and customteamcolor like the World Editor") {
	set_colour_test_rows();

	Unit tavern = placed_unit("ntav");
	tavern.custom_color = -1;
	Unit waygate = placed_unit("nwgt");
	waygate.custom_color = 3;
	Unit waygate_default = placed_unit("nwgt");
	waygate_default.custom_color = -1;
	Unit footman = placed_unit("hfoo");
	footman.custom_color = 0;

	CHECK(contains(script_for(tavern, ScriptMode::lua), "\tSetUnitColor(u, ConvertPlayerColor(0))\n"));
	CHECK(contains(script_for(tavern, ScriptMode::jass), "\tcall SetUnitColor(u, ConvertPlayerColor(0))\n"));

	CHECK(contains(script_for(waygate, ScriptMode::lua), "\tSetUnitColor(u, ConvertPlayerColor(3))\n"));
	CHECK(contains(script_for(waygate, ScriptMode::jass), "\tcall SetUnitColor(u, ConvertPlayerColor(3))\n"));

	CHECK_FALSE(contains(script_for(waygate_default, ScriptMode::lua), "SetUnitColor"));
	CHECK_FALSE(contains(script_for(waygate_default, ScriptMode::jass), "SetUnitColor"));

	CHECK_FALSE(contains(script_for(footman, ScriptMode::lua), "SetUnitColor"));
	CHECK_FALSE(contains(script_for(footman, ScriptMode::jass), "SetUnitColor"));
}

TEST_CASE("SetUnitColor comes before SelectHeroSkill") {
	units_slk.set_shadow_data("teamcolor", "Hpal", "2");
	Unit hero = placed_unit("Hpal");
	hero.abilities.emplace_back("AHhb", 0, 1);

	for (const ScriptMode mode : { ScriptMode::lua, ScriptMode::jass }) {
		const std::string script = script_for(hero, mode);
		const size_t colour = script.find("SetUnitColor(u, ConvertPlayerColor(2))");
		const size_t skill = script.find("SelectHeroSkill(");
		REQUIRE(colour != std::string::npos);
		REQUIRE(skill != std::string::npos);
		CHECK(colour < skill);
	}
}

TEST_CASE("gold mines get SetResourceAmount with the stored gold, other units do not") {
	units_slk.set_shadow_data("abillist", "ngol", "Agld,Avul");
	units_slk.set_shadow_data("abillist", "hfoo", "");

	Unit mine = placed_unit("ngol");
	mine.gold = 12500;
	Unit rich_mine = placed_unit("ngol");
	rich_mine.gold = 20000;
	Unit footman = placed_unit("hfoo");
	footman.gold = 20000;

	CHECK(contains(script_for(mine, ScriptMode::lua), "\tSetResourceAmount(u, 12500)\n"));
	CHECK(contains(script_for(mine, ScriptMode::jass), "\tcall SetResourceAmount(u, 12500)\n"));
	CHECK(contains(script_for(rich_mine, ScriptMode::lua), "\tSetResourceAmount(u, 20000)\n"));
	CHECK(contains(script_for(rich_mine, ScriptMode::jass), "\tcall SetResourceAmount(u, 20000)\n"));
	CHECK_FALSE(contains(script_for(footman, ScriptMode::lua), "SetResourceAmount"));
	CHECK_FALSE(contains(script_for(footman, ScriptMode::jass), "SetResourceAmount"));
}

TEST_CASE("ugol gets SetResourceAmount without Agld in its abilities") {
	Unit blighted = placed_unit("ugol");
	blighted.gold = 15000;
	CHECK(contains(script_for(blighted, ScriptMode::lua), "\tSetResourceAmount(u, 15000)\n"));
	CHECK(contains(script_for(blighted, ScriptMode::jass), "\tcall SetResourceAmount(u, 15000)\n"));
}

TEST_CASE("unit colour and gold use the gg_unit_ global of a unit referenced by triggers") {
	set_colour_test_rows();
	units_slk.set_shadow_data("abillist", "ngol", "Agld,Avul");
	units_slk.set_shadow_data("teamcolor", "ngol", "12");

	Unit mine = placed_unit("ngol");
	mine.creation_number = 7;
	mine.gold = 20000;
	Units units;
	units.units.push_back(mine);
	const std::unordered_map<std::string, std::string> variables { { "0007", "ngol" } };

	const std::string lua = generate_unit_script(test_terrain(), units, ScriptMode::lua, variables);
	CHECK(contains(lua, "\tSetResourceAmount(gg_unit_ngol_0007, 20000)\n"));
	CHECK(contains(lua, "\tSetUnitColor(gg_unit_ngol_0007, ConvertPlayerColor(12))\n"));

	const std::string jass = generate_unit_script(test_terrain(), units, ScriptMode::jass, variables);
	CHECK(contains(jass, "\tcall SetResourceAmount(gg_unit_ngol_0007, 20000)\n"));
	CHECK(contains(jass, "\tcall SetUnitColor(gg_unit_ngol_0007, ConvertPlayerColor(12))\n"));
}

TEST_CASE("Agld must be a whole entry of abillist, in any position") {
	units_slk.set_shadow_data("abillist", "nmg1", "Agldx,AAgld");
	units_slk.set_shadow_data("abillist", "nmg2", "Avul,Agld");

	Unit lookalike = placed_unit("nmg1");
	lookalike.gold = 20000;
	Unit last_entry = placed_unit("nmg2");
	last_entry.gold = 20000;

	CHECK_FALSE(contains(script_for(lookalike, ScriptMode::lua), "SetResourceAmount"));
	CHECK_FALSE(contains(script_for(lookalike, ScriptMode::jass), "SetResourceAmount"));
	CHECK(contains(script_for(last_entry, ScriptMode::lua), "\tSetResourceAmount(u, 20000)\n"));
	CHECK(contains(script_for(last_entry, ScriptMode::jass), "\tcall SetResourceAmount(u, 20000)\n"));
}

TEST_CASE("teamcolor is used when the custom colour does not apply") {
	units_slk.set_shadow_data("teamcolor", "ncl1", "5");
	units_slk.set_shadow_data("customteamcolor", "ncl1", "0");
	units_slk.set_shadow_data("teamcolor", "ncl2", "4");
	units_slk.set_shadow_data("customteamcolor", "ncl2", "1");

	Unit ignored_custom = placed_unit("ncl1"); // customteamcolor 0: custom_color is ignored
	ignored_custom.custom_color = 3;
	Unit no_custom = placed_unit("ncl2"); // customteamcolor 1 but no custom colour set
	no_custom.custom_color = -1;

	CHECK(contains(script_for(ignored_custom, ScriptMode::lua), "\tSetUnitColor(u, ConvertPlayerColor(5))\n"));
	CHECK(contains(script_for(ignored_custom, ScriptMode::jass), "\tcall SetUnitColor(u, ConvertPlayerColor(5))\n"));
	CHECK_FALSE(contains(script_for(ignored_custom, ScriptMode::lua), "ConvertPlayerColor(3)"));
	CHECK_FALSE(contains(script_for(ignored_custom, ScriptMode::jass), "ConvertPlayerColor(3)"));
	CHECK(contains(script_for(no_custom, ScriptMode::lua), "\tSetUnitColor(u, ConvertPlayerColor(4))\n"));
	CHECK(contains(script_for(no_custom, ScriptMode::jass), "\tcall SetUnitColor(u, ConvertPlayerColor(4))\n"));
}
