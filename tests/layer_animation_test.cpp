// doctest forward-declares std::char_traits<char>; std::string needs the real one before doctest.h when only `import std` follows.
#include <string>
#include <doctest/doctest.h>

import std;
import MDX;
import LayerAnimation;
import <glm/glm.hpp>;
import <glm/gtc/quaternion.hpp>;

using namespace layer_animation;
// mdx::TextureAnimation is not exported by the MDX module; its type is reachable through the member.
using TextureAnimation = decltype(mdx::MDX::texture_animations)::value_type;

namespace {
	template <typename T>
	mdx::TrackHeader<T> track(const mdx::InterpolationType type, const std::vector<std::pair<int, T>>& keys, const int global_sequence = -1) {
		mdx::TrackHeader<T> header;
		header.interpolation_type = type;
		header.global_sequence_ID = global_sequence;
		header.id = 0;
		for (const auto& [frame, value] : keys) {
			header.tracks.push_back({ frame, value, value, value });
		}
		return header;
	}

	TrackTime in_sequence(const std::int64_t start, const std::int64_t end, const std::int64_t frame) {
		TrackTime time;
		time.has_sequence = true;
		time.sequence_start = start;
		time.sequence_end = end;
		time.frame = frame;
		return time;
	}

	constexpr auto none = mdx::InterpolationType::none;
	constexpr auto linear = mdx::InterpolationType::linear;

	std::vector<std::size_t> order_of(const std::vector<DrawCandidate>& candidates) {
		return draw_order(candidates);
	}
} // namespace

TEST_CASE("sample_track: no keys gives the default one key or equal keys give that value") {
	const auto time = in_sequence(0, 1000, 500);
	CHECK(sample_track(track<float>(linear, {}), time, 7.f) == 7.f);
	CHECK(sample_track(track<float>(linear, { { 100, 3.f } }), time, 7.f) == 3.f);
	CHECK(sample_track(track<float>(linear, { { 0, 2.f }, { 1000, 2.f } }), time, 7.f) == 2.f);
	// Keys of another sequence do not count: no key inside [0, 1000] here.
	CHECK(sample_track(track<float>(linear, { { 2000, 4.f }, { 3000, 5.f } }), time, 7.f) == 7.f);
	// No sequence at all: the default.
	CHECK(sample_track(track<float>(linear, { { 0, 1.f }, { 1000, 2.f } }), TrackTime {}, 7.f) == 7.f);
}

TEST_CASE("sample_track: stepped keys hold until the next key and past the last one") {
	const auto steps = track<float>(none, { { 333, 0.f }, { 366, 0.125f }, { 400, 0.25f }, { 433, 0.375f } });
	CHECK(sample_track(steps, in_sequence(333, 1400, 333), -1.f) == 0.f);
	CHECK(sample_track(steps, in_sequence(333, 1400, 365), -1.f) == 0.f);
	CHECK(sample_track(steps, in_sequence(333, 1400, 366), -1.f) == 0.125f);
	CHECK(sample_track(steps, in_sequence(333, 1400, 420), -1.f) == 0.25f);
	// A stepped track that ends early (P042) holds its last key to the end of the sequence.
	CHECK(sample_track(steps, in_sequence(333, 1400, 1200), -1.f) == 0.375f);
}

TEST_CASE("sample_track: linear keys blend and outside the keys blend from the last key toward the first") {
	const auto ramp = track<float>(linear, { { 20, 0.f }, { 80, 10.f } });
	CHECK(sample_track(ramp, in_sequence(0, 100, 50), -1.f) == doctest::Approx(5.f));
	// WarMash's (and the game's) wrap: before the first key and after the last, last -> first over the rest of the sequence.
	CHECK(sample_track(ramp, in_sequence(0, 100, 90), -1.f) == doctest::Approx(7.5f));
	CHECK(sample_track(ramp, in_sequence(0, 100, 10), -1.f) == doctest::Approx(12.5f));
	// Before the sequence start (a frame the caller should not pass): the first key.
	CHECK(sample_track(ramp, in_sequence(0, 100, -5), -1.f) == 0.f);
}

TEST_CASE("sample_track: unsorted keys and odd frames never read out of bounds") {
	const auto unsorted = track<float>(none, { { 0, 0.f }, { 100, 10.f }, { 50, 5.f } });
	for (int frame = -10; frame <= 120; frame += 5) {
		const float value = sample_track(unsorted, in_sequence(0, 100, frame), -1.f);
		CHECK((value == 0.f || value == 5.f || value == 10.f));
	}
	const auto same_frame = track<float>(linear, { { 50, 1.f }, { 50, 3.f } });
	CHECK(std::isfinite(sample_track(same_frame, in_sequence(0, 100, 50), -1.f)));
	CHECK(std::isfinite(sample_track(same_frame, in_sequence(50, 50, 50), -1.f)));
	const auto extremes = track<float>(linear, { { std::numeric_limits<int>::min(), 1.f }, { std::numeric_limits<int>::max(), 3.f } });
	CHECK(std::isfinite(sample_track(extremes, in_sequence(std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), 0), -1.f)));
}

TEST_CASE("sample_track: global sequences including damaged ids and zero length") {
	const std::vector<std::uint32_t> globals = { 100, 0 };
	TrackTime time = in_sequence(1000, 2000, 1500);
	time.global_sequences = globals;
	time.global_ms = 1250; // 1250 % 100 = 50

	CHECK(sample_track(track<float>(linear, { { 0, 0.f }, { 100, 10.f } }, 0), time, -1.f) == doctest::Approx(5.f));
	// Zero-length global sequence: frame 0, no division by zero.
	CHECK(sample_track(track<float>(none, { { 0, 4.f }, { 10, 6.f } }, 1), time, -1.f) == 4.f);
	// Ids past the list and negative ids other than -1 give the default.
	CHECK(sample_track(track<float>(linear, { { 0, 0.f }, { 100, 10.f } }, 2), time, -1.f) == -1.f);
	CHECK(sample_track(track<float>(linear, { { 0, 0.f }, { 100, 10.f } }, -7), time, -1.f) == -1.f);
	// A first key past the global sequence end is held (WarMash).
	CHECK(sample_track(track<float>(linear, { { 150, 9.f }, { 200, 8.f } }, 0), time, -1.f) == 9.f);
	// A negative clock still lands inside the window.
	time.global_ms = -30;
	CHECK(sample_track(track<float>(linear, { { 0, 0.f }, { 100, 10.f } }, 0), time, -1.f) == doctest::Approx(7.f));
	// A global id in a model without global sequences reads the current sequence, as WarMash does.
	TrackTime plain = in_sequence(0, 100, 50);
	CHECK(sample_track(track<float>(linear, { { 0, 0.f }, { 100, 10.f } }, 3), plain, -1.f) == doctest::Approx(5.f));
}

TEST_CASE("texture_animation_transform samples KTAT KTAR and KTAS and damaged ids and values give identity") {
	mdx::MDX model;
	TextureAnimation animation;
	animation.KTAT = track<glm::vec3>(none, { { 0, glm::vec3(0.f, 0.f, 0.f) }, { 50, glm::vec3(0.25f, 0.5f, 9.f) } });
	animation.KTAS = track<glm::vec3>(linear, { { 0, glm::vec3(1.f) }, { 100, glm::vec3(3.f, 5.f, 1.f) } });
	model.texture_animations.push_back(animation);

	const UvTransform at60 = texture_animation_transform(model, 0, in_sequence(0, 100, 60));
	CHECK(at60.translation == glm::vec2(0.25f, 0.5f));
	CHECK(at60.rotation == glm::vec2(0.f, 1.f));
	CHECK(at60.scale.x == doctest::Approx(2.2f));
	CHECK(at60.scale.y == doctest::Approx(3.4f));

	CHECK(texture_animation_transform(model, 1, in_sequence(0, 100, 60)) == UvTransform {});
	CHECK(texture_animation_transform(model, 0xFFFFFFFFu, in_sequence(0, 100, 60)) == UvTransform {});

	TextureAnimation broken;
	broken.KTAT = track<glm::vec3>(none, { { 0, glm::vec3(std::numeric_limits<float>::quiet_NaN()) } });
	broken.KTAR = track<glm::quat>(none, { { 0, glm::quat(std::numeric_limits<float>::infinity(), 0.f, 0.f, 0.f) } });
	model.texture_animations.push_back(broken);
	CHECK(texture_animation_transform(model, 1, in_sequence(0, 100, 10)) == UvTransform {});
}

TEST_CASE("apply_uv_transform matches the shader: translate rotate and scale about the centre") {
	CHECK(apply_uv_transform({}, glm::vec2(0.3f, 0.7f)) == glm::vec2(0.3f, 0.7f));
	UvTransform shift;
	shift.translation = glm::vec2(0.f, 0.125f);
	CHECK(apply_uv_transform(shift, glm::vec2(0.5f, 0.25f)) == glm::vec2(0.5f, 0.375f));

	UvTransform quarter;
	quarter.rotation = glm::vec2(std::sqrt(0.5f), std::sqrt(0.5f)); // 90 degrees about Z
	const glm::vec2 rotated = apply_uv_transform(quarter, glm::vec2(1.f, 0.5f));
	CHECK(rotated.x == doctest::Approx(0.5f));
	CHECK(rotated.y == doctest::Approx(1.f));

	UvTransform doubled;
	doubled.scale = glm::vec2(2.f, 1.f);
	CHECK(apply_uv_transform(doubled, glm::vec2(0.75f, 0.25f)) == glm::vec2(1.f, 0.25f));
}

TEST_CASE("layer_texture_id follows a stepped KMTF flipbook and survives damaged ids") {
	mdx::LayerTexture slot;
	slot.id = 2;
	CHECK(layer_texture_id(slot, in_sequence(0, 100, 50), 5) == 2u);

	slot.KMTF = track<std::uint32_t>(none, { { 0, 0u }, { 30, 1u }, { 60, 3u } });
	CHECK(layer_texture_id(slot, in_sequence(0, 100, 10), 5) == 0u);
	CHECK(layer_texture_id(slot, in_sequence(0, 100, 30), 5) == 1u);
	CHECK(layer_texture_id(slot, in_sequence(0, 100, 99), 5) == 3u);

	// Past the texture list: the static id. Negative as a signed number: texture 0 (WarMash's max(0, id)).
	slot.KMTF = track<std::uint32_t>(none, { { 0, 9u }, { 50, 0xFFFFFFFFu } });
	CHECK(layer_texture_id(slot, in_sequence(0, 100, 10), 5) == 2u);
	CHECK(layer_texture_id(slot, in_sequence(0, 100, 60), 5) == 0u);

	// Neither the track nor the static id names a texture: nothing to draw.
	slot.id = 40;
	slot.KMTF = track<std::uint32_t>(none, { { 0, 9u } });
	CHECK_FALSE(layer_texture_id(slot, in_sequence(0, 100, 10), 5).has_value());
	CHECK_FALSE(layer_texture_id(slot, in_sequence(0, 100, 10), 0).has_value());

	// Linear flipbooks are read as numbers and cut to an integer (WarMash UInt32Sd).
	slot.id = 0;
	slot.KMTF = track<std::uint32_t>(linear, { { 0, 0u }, { 100, 4u } });
	CHECK(layer_texture_id(slot, in_sequence(0, 100, 60), 5) == 2u);
}

TEST_CASE("layer_texture_id gives texture 0 when the KMTF track has keys but none in the current window") {
	mdx::LayerTexture slot;
	slot.id = 3;
	slot.KMTF = track<std::uint32_t>(none, { { 2000, 4u }, { 3000, 1u } });
	// Keys only in another sequence: the track's own default 0 (WarMash Sd defaults), not the static id.
	CHECK(layer_texture_id(slot, in_sequence(0, 1000, 500), 5) == 0u);
	// Inside that other sequence the keys apply.
	CHECK(layer_texture_id(slot, in_sequence(2000, 3000, 2500), 5) == 4u);
	// No sequence: the static id.
	CHECK(layer_texture_id(slot, TrackTime {}, 5) == 3u);

	// Global-sequence tracks: keys in the window sample normally; a key only before the window start gives 0.
	const std::vector<std::uint32_t> globals = { 100 };
	TrackTime global_time = in_sequence(0, 1000, 500);
	global_time.global_sequences = globals;
	global_time.global_ms = 50;
	slot.KMTF = track<std::uint32_t>(none, { { 0, 2u }, { 60, 1u } }, 0);
	CHECK(layer_texture_id(slot, global_time, 5) == 2u);
	slot.KMTF = track<std::uint32_t>(none, { { -50, 4u } }, 0);
	CHECK(layer_texture_id(slot, global_time, 5) == 0u);
	slot.KMTF = track<std::uint32_t>(none, {}, 0);
	CHECK(layer_texture_id(slot, global_time, 5) == 3u); // no keys at all: the track is missing, so the static id

	// A damaged global sequence id: treated as a missing track, the static id.
	slot.KMTF = track<std::uint32_t>(none, { { 0, 2u } }, 7);
	CHECK(layer_texture_id(slot, global_time, 5) == 3u);
}

TEST_CASE("draw_order: opaque layers in model order then translucent by plane filter mode and centre Z (Shisui_BurningKunai_Glow)") {
	// Geosets 0..6 of Shisui_BurningKunai_Glow: filter mode, priority plane; every extent centre Z is 0.
	const std::vector<DrawCandidate> kunai = {
		{ 0, 0, 0, 0, 0.f }, { 1, 0, 0, 0, 0.f }, { 2, 0, 2, 1, 0.f }, { 3, 0, 2, 1, 0.f },
		{ 4, 0, 2, 2, 0.f }, { 5, 0, 3, 0, 0.f }, { 6, 0, 3, 3, 0.f },
	};
	CHECK(order_of(kunai) == std::vector<std::size_t> { 0, 1, 5, 2, 3, 4, 6 });
}

TEST_CASE("draw_order classifies each layer not the material's first layer") {
	// A translucent first layer and an opaque second layer: the opaque one still draws first.
	const std::vector<DrawCandidate> layers = {
		{ 0, 0, 2, 0, 0.f }, { 0, 1, 0, 0, 0.f }, { 1, 0, 1, 5, 0.f },
	};
	CHECK(order_of(layers) == std::vector<std::size_t> { 1, 2, 0 });
}

TEST_CASE("draw_order breaks ties by filter mode then centre Z then keeps model order") {
	const std::vector<DrawCandidate> ties = {
		{ 0, 0, 4, 1, 0.f },  // AddAlpha
		{ 1, 0, 2, 1, 5.f },  // Blend, higher Z
		{ 2, 0, 2, 1, -5.f }, // Blend, lower Z
		{ 3, 0, 2, 1, 5.f },  // same as 1: stays after it
		{ 4, 0, 2, 0, 99.f }, // lower plane first
	};
	CHECK(order_of(ties) == std::vector<std::size_t> { 4, 2, 1, 3, 0 });
}

TEST_CASE("draw_order keeps a strict weak ordering on odd planes modes and non-finite Z") {
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float inf = std::numeric_limits<float>::infinity();
	std::vector<DrawCandidate> odd;
	const std::array<float, 7> keys = { nan, inf, -inf, -0.f, 0.f, 1.f, nan };
	const std::array<std::uint32_t, 4> planes = { 0u, 0x80000000u, 0x7FFFFFFFu, 0xFFFFFFFFu };
	const std::array<std::uint32_t, 4> modes = { 2u, 7u, 0xFFFFFFFFu, 3u };
	for (int i = 0; i < 64; ++i) {
		odd.push_back({ i, 0, modes[i % modes.size()], planes[(i / 4) % planes.size()], keys[i % keys.size()] });
	}
	const auto order = order_of(odd);
	REQUIRE(order.size() == odd.size());
	std::vector<std::size_t> sorted = order;
	std::ranges::sort(sorted);
	for (std::size_t i = 0; i < sorted.size(); ++i) {
		CHECK(sorted[i] == i);
	}
	// Signed planes: 0x80000000 (int minimum) first, then -1, 0, int maximum.
	CHECK(static_cast<std::int32_t>(odd[order.front()].priority_plane) == std::numeric_limits<std::int32_t>::min());
	CHECK(odd[order.back()].priority_plane == 0x7FFFFFFFu);

	CHECK(float_compare(-0.f, 0.f) < 0);
	CHECK(float_compare(nan, inf) > 0);
	CHECK(float_compare(nan, nan) == 0);
	CHECK(float_compare(1.f, 2.f) < 0);
}

TEST_CASE("pinned: the translucent depth key is the extent centre Z as in WarMash's game renderer not its stand-alone viewer") {
	CHECK_FALSE(menu_viewer_order);
	mdx::Extent extent;
	extent.bounds_radius = 1.f;
	extent.minimum = glm::vec3(0.f, 0.f, -2.f);
	extent.maximum = glm::vec3(0.f, 0.f, 10.f);
	CHECK(translucent_depth_key(extent) == 4.f);
}

TEST_CASE("alpha test and visibility cull follow WarMash") {
	CHECK(alpha_test_threshold(0) == 0.f); // None never discards
	CHECK(alpha_test_threshold(1) == 0.75f);
	CHECK(alpha_test_threshold(2) == 0.01f);
	CHECK(alpha_test_threshold(42) == 0.01f);
	CHECK(layer_visible(0.02f, 1.f));
	CHECK_FALSE(layer_visible(0.01f, 1.f));
	CHECK_FALSE(layer_visible(1.f, 0.f));
	CHECK_FALSE(layer_visible(std::numeric_limits<float>::quiet_NaN(), 1.f));
	CHECK(is_opaque_filter(0));
	CHECK(is_opaque_filter(1));
	CHECK_FALSE(is_opaque_filter(2));
	CHECK_FALSE(is_opaque_filter(0xFFFFFFFFu));
}
