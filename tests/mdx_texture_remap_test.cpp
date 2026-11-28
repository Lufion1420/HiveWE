#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <doctest/doctest.h>

import <glm/glm.hpp>;
import MDX;

// Every place that renumbers textures (Doodad palette merge export, optimize, deduplicate_textures) must renumber a layer's
// KMTF flipbook values together with its static texture id, and an id outside the texture list must stay outside it.

namespace {
	constexpr uint32_t no_animation = 0xFFFFFFFF;

	mdx::Texture texture(const std::string& name) {
		return mdx::Texture { .replaceable_id = 0, .file_name = name, .flags = 0 };
	}

	mdx::LayerTexture slot(const uint32_t id, const std::vector<uint32_t>& flipbook, const bool hermite = false) {
		mdx::LayerTexture result {};
		result.id = id;
		result.KMTF.interpolation_type = hermite ? mdx::InterpolationType::hermite : mdx::InterpolationType::none;
		int32_t frame = 0;
		for (const uint32_t value : flipbook) {
			mdx::Track<uint32_t> track {};
			track.frame = frame;
			track.value = value;
			track.inTan = hermite ? value : 0;
			track.outTan = hermite ? value : 0;
			result.KMTF.tracks.push_back(track);
			frame += 100;
		}
		return result;
	}

	mdx::Layer layer(const mdx::LayerTexture& texture_slot, const uint32_t texture_animation_id = no_animation) {
		mdx::Layer result {};
		result.texture_animation_id = texture_animation_id;
		result.alpha = 1.f;
		result.textures.push_back(texture_slot);
		return result;
	}

	mdx::MDX model(const std::vector<std::string>& names, const std::vector<mdx::Layer>& layers) {
		mdx::MDX result;
		for (const auto& name : names) {
			result.textures.push_back(texture(name));
		}
		mdx::Material material {};
		material.layers = layers;
		result.materials.push_back(material);
		return result;
	}

	/// The texture file name each id points at, or "<out>" for an id outside the list.
	std::vector<std::string> names_of(const mdx::MDX& mdx, const mdx::LayerTexture& slot) {
		const auto name = [&](const uint32_t id) {
			return id < mdx.textures.size() ? mdx.textures[id].file_name.string() : std::string("<out>");
		};
		std::vector<std::string> result { name(slot.id) };
		for (const auto& track : slot.KMTF.tracks) {
			result.push_back(name(track.value));
		}
		return result;
	}
} // namespace

TEST_CASE("merge_with offsets KMTF flipbook values and tangents together with the static texture id") {
	mdx::MDX base;
	base.merge_with(model({ "a0", "a1" }, { layer(slot(1, { 0, 1 })) }), glm::mat4(1.f));
	base.merge_with(model({ "b0", "b1" }, { layer(slot(1, { 1, 0 }, true)) }), glm::mat4(1.f));

	REQUIRE_EQ(base.textures.size(), 4);
	REQUIRE_EQ(base.materials.size(), 2);
	const auto& first = base.materials[0].layers[0].textures[0];
	const auto& second = base.materials[1].layers[0].textures[0];
	CHECK(names_of(base, first) == std::vector<std::string> { "a1", "a0", "a1" });
	CHECK(names_of(base, second) == std::vector<std::string> { "b1", "b1", "b0" });
	REQUIRE_EQ(second.KMTF.tracks.size(), 2);
	CHECK_EQ(second.KMTF.tracks[0].inTan, 3);
	CHECK_EQ(second.KMTF.tracks[0].outTan, 3);
	CHECK_EQ(second.KMTF.tracks[1].inTan, 2);
	CHECK_EQ(second.KMTF.tracks[1].outTan, 2);
}

TEST_CASE("merge_with keeps texture ids outside the merged model's list outside the result") {
	mdx::MDX base;
	base.merge_with(model({ "a0", "a1" }, { layer(slot(0, {})) }), glm::mat4(1.f));
	base.merge_with(model({ "b0", "b1" }, { layer(slot(0xFFFFFFFF, { 7, 0xFFFFFFFE, 1 })) }), glm::mat4(1.f));

	REQUIRE_EQ(base.textures.size(), 4);
	CHECK(names_of(base, base.materials[1].layers[0].textures[0]) == std::vector<std::string> { "<out>", "<out>", "<out>", "b1" });
}

TEST_CASE("merge_with leaves a layer without texture animation without one and offsets the others") {
	mdx::MDX first = model({ "a0" }, { layer(slot(0, {})) });
	first.texture_animations.resize(1);
	mdx::MDX second = model({ "b0" }, { layer(slot(0, {})), layer(slot(0, {}), 1), layer(slot(0, {}), 5) });
	second.texture_animations.resize(2);

	mdx::MDX base;
	base.merge_with(first, glm::mat4(1.f));
	base.merge_with(second, glm::mat4(1.f));

	REQUIRE_EQ(base.materials.size(), 2);
	CHECK_EQ(base.materials[0].layers[0].texture_animation_id, no_animation);
	const auto& layers = base.materials[1].layers;
	REQUIRE_EQ(layers.size(), 3);
	CHECK_EQ(layers[0].texture_animation_id, no_animation);
	CHECK_EQ(layers[1].texture_animation_id, 2); // second model's animation 1 after the first model's one
	CHECK_EQ(layers[2].texture_animation_id, no_animation); // id 5 pointed past the second model's list
	CHECK_EQ(base.texture_animations.size(), 3);
}

TEST_CASE("optimize keeps textures used only by a KMTF flipbook and renumbers the flipbook") {
	// Texture 1 is unused, texture 0 is used only by the flipbook.
	mdx::MDX mdx = model({ "t0", "t1", "t2" }, { layer(slot(2, { 0, 2 }, true)) });
	mdx.optimize(0.f);

	REQUIRE_EQ(mdx.textures.size(), 2);
	const auto& result = mdx.materials[0].layers[0].textures[0];
	CHECK(names_of(mdx, result) == std::vector<std::string> { "t2", "t0", "t2" });
	CHECK_EQ(result.KMTF.tracks[0].inTan, 0);
	CHECK_EQ(result.KMTF.tracks[1].outTan, 1);
}

TEST_CASE("optimize keeps texture ids outside the list outside it") {
	mdx::MDX mdx = model({ "t0", "t1", "t2" }, { layer(slot(9, { 2, 0xFFFFFFFF, 5000 })) });
	mdx.optimize(0.f);

	REQUIRE_EQ(mdx.textures.size(), 1);
	CHECK(names_of(mdx, mdx.materials[0].layers[0].textures[0]) == std::vector<std::string> { "<out>", "t2", "<out>", "<out>" });
}

TEST_CASE("deduplicate_textures points static ids and KMTF flipbooks at the same files as before") {
	mdx::MDX mdx = model(
		{ "t0", "t1", "t0", "t3", "t4", "t5", "t3", "t7" },
		{ layer(slot(2, { 0, 1, 2, 3, 4, 5, 6, 7 })), layer(slot(7, { 6, 5 }, true)), layer(slot(12, { 3, 0xFFFFFFFF })) }
	);
	const auto before0 = names_of(mdx, mdx.materials[0].layers[0].textures[0]);
	const auto before1 = names_of(mdx, mdx.materials[0].layers[1].textures[0]);

	mdx.deduplicate_textures();

	CHECK_EQ(mdx.textures.size(), 6);
	CHECK(names_of(mdx, mdx.materials[0].layers[0].textures[0]) == before0);
	CHECK(names_of(mdx, mdx.materials[0].layers[1].textures[0]) == before1);
	CHECK(names_of(mdx, mdx.materials[0].layers[2].textures[0]) == std::vector<std::string> { "<out>", "t3", "<out>" });
	for (const auto& track : mdx.materials[0].layers[1].textures[0].KMTF.tracks) {
		CHECK_EQ(track.inTan, track.value);
		CHECK_EQ(track.outTan, track.value);
	}
}
