#include <string>
#include <doctest/doctest.h>

#include "../src/menus/selection_hit_points.h"

TEST_CASE("selection hit points: -1 is the default, 100 percent of the SLK hp") {
	CHECK(selection_hit_points_percent(-1) == 100);
	CHECK(selection_hit_points_text(-1, 420) == "100% (420)");
}

TEST_CASE("selection hit points: Unit::health is a percentage of the maximum, not an absolute value") {
	CHECK(selection_hit_points_percent(50) == 50);
	CHECK(selection_hit_points_text(50, 420) == "50% (210)");
	CHECK(selection_hit_points_text(33, 100) == "33% (33)");
}

TEST_CASE("selection hit points: without a known maximum only the percentage is shown") {
	CHECK(selection_hit_points_text(-1, 0) == "100%");
	CHECK(selection_hit_points_text(75, 0) == "75%");
}
