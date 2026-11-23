// doctest forward-declares std::char_traits<char>; std::string needs the real one before doctest.h when only `import std` follows.
#include <string>
#include <doctest/doctest.h>

import std;
import ModelCatalog;
import TimelineMath;

namespace fs = std::filesystem;

TEST_CASE("is_model_file accepts mdx in any letter case and refuses portraits and temp files") {
	CHECK(is_model_file(fs::path("units/human/footman/footman.mdx")));
	CHECK(is_model_file(fs::path("Units/Human/Footman/Footman.MDX")));
	CHECK(is_model_file(fs::path("a.Mdx")));
	CHECK_FALSE(is_model_file(fs::path("units/human/footman/footman_portrait.mdx")));
	CHECK_FALSE(is_model_file(fs::path("units/human/footman/Footman_Portrait.MDX")));
	CHECK_FALSE(is_model_file(fs::path("units/human/footman/footman.mdx.hivewe-tmp")));
	CHECK_FALSE(is_model_file(fs::path("units/human/footman/footman.mdl")));
	CHECK_FALSE(is_model_file(fs::path("units/human/footman/footman.blp")));
}

TEST_CASE("game_model_category reads the first folder and ignores the war3.w3mod prefix") {
	CHECK(game_model_category("war3.w3mod:units/human/footman/footman.mdx") == ModelCategory::Units);
	CHECK(game_model_category("Buildings\\Human\\Farm\\Farm.mdx") == ModelCategory::Buildings);
	CHECK(game_model_category("sharedmodels/x.mdx") == ModelCategory::SharedModels);
	CHECK(game_model_category("abilities/spells/human/x.mdx") == ModelCategory::Abilities);
	CHECK_FALSE(game_model_category("textures/x.mdx").has_value());
	CHECK_FALSE(game_model_category("loose.mdx").has_value());
	CHECK_FALSE(game_model_category("unitsextra/x.mdx").has_value());
}

TEST_CASE("every game source glob maps back to its own category") {
	for (const auto& source : game_category_sources) {
		const std::string sample = std::string(source.glob.substr(0, source.glob.find('*'))) + "x.mdx";
		CHECK(game_model_category(sample) == source.category);
	}
}

TEST_CASE("build_model_catalog counts per category and lets map models win") {
	const std::vector<std::string> map_files {
		"Units/Human/Footman/Footman.mdx", // overrides the game model below, different letter case
		"custom\\thing.mdx",
		"custom/thing_portrait.mdx",
		"custom/thing.mdx.hivewe-tmp",
		"custom/readme.txt",
		"custom/THING.MDX", // same path as custom\thing.mdx
	};
	const std::vector<std::string> game_files {
		"war3.w3mod:units/human/footman/footman.mdx",
		"war3.w3mod:units/human/knight/knight.mdx",
		"war3.w3mod:units/human/knight/knight.mdx", // repeated
		"war3.w3mod:units/human/knight/knight_portrait.mdx",
		"war3.w3mod:doodads/trees/tree.MDX",
		"war3.w3mod:ui/misc/whatever.mdx", // not a model folder
	};

	const ModelCatalog catalog = build_model_catalog(map_files, game_files);

	CHECK(catalog.total() == 4);
	CHECK(catalog.count(ModelCategory::Map) == 2);
	CHECK(catalog.count(ModelCategory::Units) == 1);
	CHECK(catalog.count(ModelCategory::Doodads) == 1);
	CHECK(catalog.count(ModelCategory::Buildings) == 0);

	REQUIRE(catalog.entries.size() == 4);
	CHECK(catalog.entries[0].category == ModelCategory::Map);
	CHECK(catalog.entries[0].path.generic_string() == "Units/Human/Footman/Footman.mdx");
	CHECK(catalog.entries[1].path.generic_string() == "custom/thing.mdx");
	CHECK(catalog.entries[2].category == ModelCategory::Units);
	CHECK(catalog.entries[2].path.generic_string() == "units/human/knight/knight.mdx");
	CHECK(catalog.entries[3].path.generic_string() == "doodads/trees/tree.MDX");
}

TEST_CASE("build_model_catalog of nothing is empty") {
	const ModelCatalog catalog = build_model_catalog({}, {});
	CHECK(catalog.total() == 0);
	CHECK(catalog.count(ModelCategory::Map) == 0);
}

TEST_CASE("advance_frame loops, stops and respects speed") {
	// 1000 ms sequence, 0.5 s at 1x
	auto result = advance_frame(100.0, 0.0, 1000.0, true, 1.0, 0.5);
	CHECK(result.frame == doctest::Approx(600.0));
	CHECK_FALSE(result.finished);

	// speed scales the step
	result = advance_frame(100.0, 0.0, 1000.0, true, 0.25, 0.4);
	CHECK(result.frame == doctest::Approx(200.0));
	result = advance_frame(100.0, 0.0, 1000.0, true, 2.0, 0.1);
	CHECK(result.frame == doctest::Approx(300.0));

	// loop on: wraps into the sequence, keeping the overshoot
	result = advance_frame(900.0, 100.0, 1100.0, true, 1.0, 0.3); // 1200 is 100 past the end, so 100 into the sequence
	CHECK(result.frame == doctest::Approx(200.0));
	CHECK_FALSE(result.finished);
	CHECK(result.wrapped);

	// loop off: clamps on the last frame and reports it
	result = advance_frame(900.0, 100.0, 1100.0, false, 1.0, 0.3);
	CHECK(result.frame == doctest::Approx(1100.0));
	CHECK(result.finished);
	CHECK_FALSE(result.wrapped);

	// no wrap flag while inside the sequence, or when nothing moves
	CHECK_FALSE(advance_frame(100.0, 0.0, 1000.0, true, 1.0, 0.5).wrapped);
	CHECK_FALSE(advance_frame(500.0, 0.0, 1000.0, true, 0.0, 5.0).wrapped);
	// landing exactly on the last frame is not a wrap yet
	CHECK_FALSE(advance_frame(500.0, 0.0, 1000.0, true, 1.0, 0.5).wrapped);

	// no movement for speed 0, no delta, or an empty sequence
	CHECK(advance_frame(500.0, 0.0, 1000.0, true, 0.0, 1.0).frame == doctest::Approx(500.0));
	CHECK(advance_frame(500.0, 0.0, 1000.0, true, 1.0, 0.0).frame == doctest::Approx(500.0));
	CHECK(advance_frame(5.0, 5.0, 5.0, true, 1.0, 1.0).frame == doctest::Approx(5.0));
}

TEST_CASE("frames outside the sequence are clamped") {
	CHECK(clamp_frame(-50.0, 0.0, 1000.0) == doctest::Approx(0.0));
	CHECK(clamp_frame(5000.0, 0.0, 1000.0) == doctest::Approx(1000.0));
	CHECK(clamp_frame(10.0, 100.0, 50.0) == doctest::Approx(100.0)); // inverted range collapses to the start
	// a frame left over from the previous sequence is pulled into the new range before advancing
	CHECK(advance_frame(5000.0, 0.0, 1000.0, false, 1.0, 0.1).frame == doctest::Approx(1000.0));
}

TEST_CASE("slider and readout conversions") {
	CHECK(slider_to_frame(0, 1000, 200.0, 1200.0) == doctest::Approx(200.0));
	CHECK(slider_to_frame(500, 1000, 200.0, 1200.0) == doctest::Approx(700.0));
	CHECK(slider_to_frame(5000, 1000, 200.0, 1200.0) == doctest::Approx(1200.0));
	CHECK(slider_to_frame(-5, 1000, 200.0, 1200.0) == doctest::Approx(200.0));
	CHECK(slider_to_frame(3, 0, 200.0, 1200.0) == doctest::Approx(200.0));

	CHECK(frame_to_slider(700.0, 200.0, 1200.0, 1000) == 500);
	CHECK(frame_to_slider(99999.0, 200.0, 1200.0, 1000) == 1000);
	CHECK(frame_to_slider(5.0, 200.0, 1200.0, 1000) == 0);
	CHECK(frame_to_slider(5.0, 200.0, 200.0, 1000) == 0);

	CHECK(format_readout(540.0, 200.0, 1200.0) == "340 / 1000 ms");
	CHECK(format_readout(99999.0, 0.0, 2167.0) == "2167 / 2167 ms");
	CHECK(format_speed(1.0) == "1.0x");
	CHECK(format_speed(0.25) == "0.25x");
}
