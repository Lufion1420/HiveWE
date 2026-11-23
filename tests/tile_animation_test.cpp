// doctest forward-declares std::char_traits<char>; std::string needs the real one before doctest.h when only `import std` follows.
#include <string>
#include <doctest/doctest.h>

import std;
import TileAnimation;
import TimelineMath;

namespace {
	std::vector<SequenceCandidate> named(const std::vector<std::string_view>& names) {
		std::vector<SequenceCandidate> result;
		for (const auto name : names) {
			result.push_back({ name, true });
		}
		return result;
	}
} // namespace

TEST_CASE("preferred_preview_sequence takes a plain Stand first") {
	CHECK(preferred_preview_sequence(named({ "Walk", "Stand Alternate", "Stand", "Attack" })) == 2);
	CHECK(preferred_preview_sequence(named({ "Walk", "STAND - 1", "Stand" })) == 1);
	CHECK(preferred_preview_sequence(named({ "stand" })) == 0);
}

TEST_CASE("preferred_preview_sequence falls back to a stand variant, then to the first sequence") {
	CHECK(preferred_preview_sequence(named({ "Walk", "Stand Ready", "Stand Alternate" })) == 1);
	CHECK(preferred_preview_sequence(named({ "Birth", "Death", "Walk" })) == 0);
}

TEST_CASE("preferred_preview_sequence is -1 without a usable sequence") {
	CHECK(preferred_preview_sequence({}) == -1);
	const std::vector<SequenceCandidate> all_empty { { "Stand", false }, { "Death", false } };
	CHECK(preferred_preview_sequence(all_empty) == -1);
}

TEST_CASE("preferred_preview_sequence skips unusable sequences at every step") {
	// A spell effect with an empty Stand placeholder plays its Birth instead.
	const std::vector<SequenceCandidate> placeholder_stand { { "Stand", false }, { "Birth", true }, { "Stand Work", true } };
	CHECK(preferred_preview_sequence(placeholder_stand) == 2);
	const std::vector<SequenceCandidate> only_birth { { "Stand", false }, { "Death", false }, { "Birth", true } };
	CHECK(preferred_preview_sequence(only_birth) == 2);
}

TEST_CASE("choose_animated_rows keeps the visible rows in order up to the cap") {
	const std::vector<int> visible { 4, 5, 6, 7, 8, 9 };
	CHECK(choose_animated_rows(visible, -1, -1, 4) == std::vector<int> { 4, 5, 6, 7 });
	CHECK(choose_animated_rows(visible, -1, -1, 20) == visible);
	CHECK(choose_animated_rows(visible, -1, -1, 0).empty());
	CHECK(choose_animated_rows({}, 3, 3, 12).empty());
}

TEST_CASE("choose_animated_rows puts the hovered, then the selected row first, without duplicates") {
	const std::vector<int> visible { 0, 1, 2, 3, 4, 5 };
	CHECK(choose_animated_rows(visible, 5, 4, 3) == std::vector<int> { 5, 4, 0 });
	CHECK(choose_animated_rows(visible, 2, 2, 3) == std::vector<int> { 2, 0, 1 });
}

TEST_CASE("choose_animated_rows ignores a hovered or selected row that is not visible") {
	const std::vector<int> visible { 10, 11, 12 };
	CHECK(choose_animated_rows(visible, 3, 99, 2) == std::vector<int> { 10, 11 });
}

TEST_CASE("next_tile_frame loops and limits a late step") {
	const FrameAdvance step = next_tile_frame(100.0, 100.0, 1100.0, 1.0 / animated_tile_fps);
	CHECK(step.frame == doctest::Approx(100.0 + 1000.0 / animated_tile_fps));
	CHECK_FALSE(step.wrapped);

	const FrameAdvance wrap = next_tile_frame(1050.0, 100.0, 1100.0, 0.1);
	CHECK(wrap.wrapped);
	CHECK(wrap.frame == doctest::Approx(150.0));

	// A 5 second stall moves the tile by max_tile_step_seconds only.
	const FrameAdvance late = next_tile_frame(100.0, 100.0, 10100.0, 5.0);
	CHECK(late.frame == doctest::Approx(100.0 + max_tile_step_seconds * 1000.0));
	CHECK_FALSE(late.finished);

	// A sequence without length stays where it is.
	CHECK(next_tile_frame(300.0, 300.0, 300.0, 0.1).frame == doctest::Approx(300.0));
}
