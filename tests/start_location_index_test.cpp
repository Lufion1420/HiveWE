#include <doctest/doctest.h>

import std;
import MapInfo;
import Triggers;

TEST_CASE("start location indices follow players order, not player numbers") {
	// ForgottenHollow: players 1 and 10. The game wants start locations 0 and 1.
	MapInfo info;
	info.players.resize(2);
	info.players[0].internal_number = 1;
	info.players[1].internal_number = 10;

	const auto indices = start_location_indices(info);
	REQUIRE(indices.size() == 2);
	CHECK(indices.at(1) == 0);
	CHECK(indices.at(10) == 1);
}

TEST_CASE("start location index is positional even for a non-ascending player list") {
	MapInfo info;
	info.players.resize(3);
	info.players[0].internal_number = 5;
	info.players[1].internal_number = 0;
	info.players[2].internal_number = 11;

	const auto indices = start_location_indices(info);
	CHECK(indices.at(5) == 0);
	CHECK(indices.at(0) == 1);
	CHECK(indices.at(11) == 2);
}
