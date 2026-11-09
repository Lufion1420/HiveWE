#include <doctest/doctest.h>

import std;
import <glm/glm.hpp>;
import Terrain;
import Triggers;
import Units;

namespace {

	const Terrain& test_terrain() {
		static const Terrain* terrain = new Terrain(); // Never destroyed: its destructor calls GL.
		return *terrain;
	}

	void add_item(Units& units, const std::string& id, const int creation_number, const float x, const float y) {
		Unit item;
		item.id = id;
		item.skin_id = id;
		item.position = glm::vec3(x, y, 0.f);
		item.creation_number = creation_number;
		units.items.push_back(item);
	}

	bool contains(const std::string& haystack, const std::string_view needle) {
		return haystack.find(needle) != std::string::npos;
	}

} // namespace

TEST_CASE("the trigger scan finds gg_item_ references with variable-length numbers") {
	const std::string trigger_script = "udg_Item = gg_item_ergg_0000\n"
									   "call PortalMechanics.addOrbItem(1, gg_item_ergp_12345)\n"
									   "set x = gg_item_ergs_0002)\n";
	const auto items = scan_item_variables(trigger_script);

	CHECK(items.size() == 3);
	CHECK(items.at("0000") == "ergg");
	CHECK(items.at("12345") == "ergp");
	CHECK(items.at("0002") == "ergs");
	CHECK(item_global_name("ergg", 0) == "gg_item_ergg_0000");
	CHECK(item_global_name("ergp", 12345) == "gg_item_ergp_12345");
}

TEST_CASE("the trigger scan ignores truncated or numberless gg_item_ text") {
	CHECK(scan_item_variables("gg_item_").empty());
	CHECK(scan_item_variables("gg_item_ergg").empty());
	CHECK(scan_item_variables("gg_item_ergg_").empty());
	CHECK(scan_item_variables("gg_item_ergg_x").empty());
	CHECK(scan_item_variables("gg_item_ergg_7").at("7") == "ergg");
}

TEST_CASE("referenced items get a declared global and are assigned in CreateAllItems") {
	Units units;
	add_item(units, "ergg", 0, 1.f, 2.f);
	add_item(units, "ergp", 12345, 3.f, 4.f);
	add_item(units, "ratc", 7, 5.f, 6.f); // Not referenced: created without a global.

	const auto items = scan_item_variables("udg_Item = gg_item_ergg_0000\nudg_Other = gg_item_ergp_12345\n");

	const std::string lua = generate_item_script(items, test_terrain(), units, ScriptMode::lua);
	CHECK(contains(lua, "gg_item_ergg_0000 = nil\n"));
	CHECK(contains(lua, "gg_item_ergp_12345 = nil\n"));
	CHECK(contains(lua, "gg_item_ergg_0000 = BlzCreateItemWithSkin(FourCC(\"ergg\"), 128, 256, FourCC(\"ergg\"))\n"));
	CHECK(contains(lua, "gg_item_ergp_12345 = BlzCreateItemWithSkin(FourCC(\"ergp\"), 384, 512, FourCC(\"ergp\"))\n"));
	CHECK(contains(lua, "\tBlzCreateItemWithSkin(FourCC(\"ratc\"), 640, 768, FourCC(\"ratc\"))\n"));
	CHECK_FALSE(contains(lua, "gg_item_ratc"));

	const std::string jass = generate_item_script(items, test_terrain(), units, ScriptMode::jass);
	CHECK(contains(jass, "item gg_item_ergg_0000 = null\n"));
	CHECK(contains(jass, "item gg_item_ergp_12345 = null\n"));
	CHECK(contains(jass, "set gg_item_ergg_0000 = BlzCreateItemWithSkin('ergg', 128, 256, 'ergg')\n"));
	CHECK(contains(jass, "set gg_item_ergp_12345 = BlzCreateItemWithSkin('ergp', 384, 512, 'ergp')\n"));
	CHECK(contains(jass, "call BlzCreateItemWithSkin('ratc', 640, 768, 'ratc')\n"));
	CHECK_FALSE(contains(jass, "gg_item_ratc"));
}

TEST_CASE("the trigger scan skips matches whose type holds non-identifier characters") {
	CHECK(scan_item_variables("gg_item_a.b(_5").empty());
	CHECK(scan_item_variables("gg_item_a b _5").empty());
	CHECK(scan_item_variables("gg_item_A_0z_5").at("5") == "A_0z");
}

TEST_CASE("a stale reference with another type does not assign the placed item") {
	Units units;
	add_item(units, "ergg", 7, 1.f, 2.f);

	const auto items = scan_item_variables("udg_Item = gg_item_ratc_0007\n");

	const std::string lua = generate_item_script(items, test_terrain(), units, ScriptMode::lua);
	CHECK(contains(lua, "gg_item_ratc_0007 = nil\n"));
	CHECK(contains(lua, "\tBlzCreateItemWithSkin(FourCC(\"ergg\"), 128, 256, FourCC(\"ergg\"))\n"));
	CHECK_FALSE(contains(lua, "gg_item_ergg_0007"));

	const std::string jass = generate_item_script(items, test_terrain(), units, ScriptMode::jass);
	CHECK(contains(jass, "item gg_item_ratc_0007 = null\n"));
	CHECK(contains(jass, "call BlzCreateItemWithSkin('ergg', 128, 256, 'ergg')\n"));
	CHECK_FALSE(contains(jass, "gg_item_ergg_0007"));
}
