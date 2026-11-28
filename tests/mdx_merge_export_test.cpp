#define DOCTEST_CONFIG_USE_STD_HEADERS
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <numeric>
#include <string>
#include <vector>
#include <doctest/doctest.h>

import <glm/glm.hpp>;
import MDX;
import MdxMergeExport;
import BinaryReader;
import BinaryWriter;

// The Doodad palette's "merge selection" export (MDX::merged_for_export): merge_with per doodad, then
// deduplicate_textures, deduplicate_materials, deduplicate_geosets, calculate_extents, to_mdx. The written file must be
// readable by the real reader, every face index must be inside its geoset, and every merged model's vertices must
// still be drawn by faces of the same geoset.

namespace {
	constexpr uint32_t outside = std::numeric_limits<uint32_t>::max();

	mdx::Material material(const uint32_t texture_id, const uint32_t blend_mode = 0) {
		mdx::Layer layer {};
		layer.blend_mode = blend_mode;
		layer.texture_animation_id = outside;
		layer.alpha = 1.f;
		mdx::LayerTexture slot {};
		slot.id = texture_id;
		layer.textures.push_back(slot);
		mdx::Material result {};
		result.layers.push_back(layer);
		return result;
	}

	/// A geoset of `triangles` separate triangles (3 vertices each), every vertex offset by `origin`.
	mdx::Geoset geoset(const uint32_t material_id, const size_t triangles, const glm::vec3 origin = glm::vec3(0.f)) {
		mdx::Geoset g {};
		g.material_id = material_id;
		g.uv_sets.resize(1);
		for (size_t t = 0; t < triangles; t++) {
			const glm::vec3 base = origin + glm::vec3(static_cast<float>(t), 0.f, 0.f);
			for (const glm::vec3 corner : { glm::vec3(0.f), glm::vec3(1.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f) }) {
				g.faces.push_back(static_cast<uint16_t>(g.vertices.size()));
				g.vertices.push_back(base + corner);
				g.normals.push_back(glm::vec3(0.f, 0.f, 1.f));
				g.uv_sets[0].push_back(glm::vec2(corner));
			}
		}
		g.face_type_groups = { 4 };
		g.face_groups = { static_cast<uint32_t>(g.faces.size()) };
		g.sequence_extents.resize(1);
		return g;
	}

	mdx::MDX model(const std::vector<std::string>& textures, const std::vector<mdx::Material>& materials,
				   const std::vector<mdx::Geoset>& geosets) {
		mdx::MDX result {};
		for (const auto& name : textures) {
			result.textures.push_back(mdx::Texture { .replaceable_id = 0, .file_name = name, .flags = 0 });
		}
		result.materials = materials;
		result.geosets = geosets;
		return result;
	}

	mdx::MDX tree() {
		return model({ "tree.blp" }, { material(0) }, { geoset(0, 2) });
	}

	mdx::MDX rock() {
		return model({ "rock.blp" }, { material(0, 1) }, { geoset(0, 1, glm::vec3(0.f, 0.f, 5.f)) });
	}

	glm::mat4 translate(const glm::vec3 offset) {
		glm::mat4 result(1.f);
		result[3] = glm::vec4(offset, 1.f);
		return result;
	}

	mdx::MDX reread(const mdx::MDX& source) {
		const BinaryWriter writer = source.to_mdx();
		BinaryReader reader(writer.buffer);
		return mdx::MDX(reader);
	}

	/// Every face index in range, face groups that agree with the faces, and per-vertex arrays of the vertex count.
	void check_well_formed(const mdx::MDX& m) {
		for (size_t i = 0; i < m.geosets.size(); i++) {
			INFO("geoset " << i);
			const auto& g = m.geosets[i];
			const size_t n = g.vertices.size();
			CHECK(g.material_id < m.materials.size());
			CHECK_EQ(g.normals.size(), n);
			for (const auto& set : g.uv_sets) {
				CHECK_EQ(set.size(), n);
			}
			CHECK((g.skin.empty() || g.skin.size() == n * 8));
			CHECK((g.tangents.empty() || g.tangents.size() == n));
			CHECK_EQ(g.faces.size() % 3, 0);
			for (const uint16_t face : g.faces) {
				REQUIRE(face < n);
			}
			CHECK_EQ(g.face_type_groups.size(), g.face_groups.size());
			CHECK_EQ(std::accumulate(g.face_groups.begin(), g.face_groups.end(), uint64_t { 0 }), g.faces.size());
		}
		for (const auto& animation : m.animations) {
			CHECK(animation.geoset_id < m.geosets.size());
		}
	}

	/// True when `g` holds a triangle with exactly these corner positions, drawn by one of its faces.
	bool draws_triangle(const mdx::Geoset& g, const glm::vec3 a, const glm::vec3 b, const glm::vec3 c) {
		const auto near = [](const glm::vec3 x, const glm::vec3 y) {
			return glm::all(glm::lessThan(glm::abs(x - y), glm::vec3(1e-3f)));
		};
		for (size_t f = 0; f + 2 < g.faces.size(); f += 3) {
			if (g.faces[f + 2] >= g.vertices.size()) {
				return false;
			}
			if (near(g.vertices[g.faces[f]], a) && near(g.vertices[g.faces[f + 1]], b) && near(g.vertices[g.faces[f + 2]], c)) {
				return true;
			}
		}
		return false;
	}

	std::string texture_of(const mdx::MDX& m, const mdx::Geoset& g) {
		const uint32_t id = m.materials.at(g.material_id).layers.at(0).textures.at(0).id;
		return m.textures.at(id).file_name.string();
	}
} // namespace

TEST_CASE("deduplicate_materials keeps first occurrences in order with compact ids") {
	const mdx::Material a = material(0);
	const mdx::Material b = material(1);
	const mdx::Material c = material(2);
	mdx::MDX m = model({ "t0", "t1", "t2" }, { a, a, b, c, b }, { geoset(0, 1), geoset(1, 1), geoset(2, 1), geoset(4, 1), geoset(3, 1) });
	m.ribbons.emplace_back().material_id = 4;

	m.deduplicate_materials();

	REQUIRE_EQ(m.materials.size(), 3);
	CHECK(m.materials[0] == a);
	CHECK(m.materials[1] == b);
	CHECK(m.materials[2] == c);
	CHECK_EQ(m.geosets[0].material_id, 0);
	CHECK_EQ(m.geosets[1].material_id, 0);
	CHECK_EQ(m.geosets[2].material_id, 1);
	CHECK_EQ(m.geosets[3].material_id, 1);
	CHECK_EQ(m.geosets[4].material_id, 2);
	CHECK_EQ(m.ribbons[0].material_id, 1);
}

TEST_CASE("deduplicate_materials with [A, A, B] gives B id 1 of 2") {
	mdx::MDX m = model({ "t0", "t1" }, { material(0), material(0), material(1) }, { geoset(2, 1) });
	m.deduplicate_materials();
	REQUIRE_EQ(m.materials.size(), 2);
	CHECK_EQ(m.geosets[0].material_id, 1);
	CHECK(m.materials[1] == material(1));
}

TEST_CASE("deduplicate_materials leaves a material id outside the list outside it") {
	mdx::MDX m = model({ "t0" }, { material(0), material(0) }, { geoset(7, 1), geoset(outside, 1) });
	m.deduplicate_materials();
	REQUIRE_EQ(m.materials.size(), 1);
	CHECK(m.geosets[0].material_id >= m.materials.size());
	CHECK(m.geosets[1].material_id >= m.materials.size());
}

TEST_CASE("merge export of two identical trees and a rock keeps every model drawn") {
	const mdx::MDX first_tree = tree();
	const mdx::MDX second_tree = tree();
	const mdx::MDX the_rock = rock();
	const glm::vec3 shift(100.f, 0.f, 0.f);

	const mdx::MDX merged = mdx::merged_for_export({
		{ &first_tree, glm::mat4(1.f) },
		{ &second_tree, translate(shift) },
		{ &the_rock, glm::mat4(1.f) },
	});
	const mdx::MDX result = reread(merged);

	check_well_formed(result);
	CHECK_EQ(result.textures.size(), 2);
	CHECK_EQ(result.materials.size(), 2);
	REQUIRE_EQ(result.geosets.size(), 2);

	const auto& trees = result.geosets[0];
	const auto& rocks = result.geosets[1];
	CHECK_EQ(texture_of(result, trees), "tree.blp");
	CHECK_EQ(texture_of(result, rocks), "rock.blp");
	CHECK_EQ(trees.vertices.size(), 12);
	CHECK_EQ(trees.faces.size(), 12);
	CHECK_EQ(rocks.vertices.size(), 3);

	for (const glm::vec3 offset : { glm::vec3(0.f), shift }) {
		INFO("tree at x " << offset.x);
		for (float t = 0.f; t < 2.f; t += 1.f) {
			const glm::vec3 base = offset + glm::vec3(t, 0.f, 0.f);
			CHECK(draws_triangle(trees, base, base + glm::vec3(1.f, 0.f, 0.f), base + glm::vec3(0.f, 1.f, 0.f)));
		}
	}
	CHECK(draws_triangle(rocks, glm::vec3(0.f, 0.f, 5.f), glm::vec3(1.f, 0.f, 5.f), glm::vec3(0.f, 1.f, 5.f)));

	// Every vertex is used by some face of its geoset
	for (const auto& g : result.geosets) {
		std::vector<bool> used(g.vertices.size(), false);
		for (const uint16_t face : g.faces) {
			used[face] = true;
		}
		CHECK(std::ranges::all_of(used, [](const bool u) { return u; }));
	}

	CHECK(result.extent.minimum.x <= 0.f);
	CHECK(result.extent.maximum.x >= 101.f);
}

TEST_CASE("merge export rotates normals and tangents with the model") {
	mdx::MDX source = tree();
	source.geosets[0].normals.assign(source.geosets[0].vertices.size(), glm::vec3(1.f, 0.f, 0.f));
	source.geosets[0].tangents.assign(source.geosets[0].vertices.size(), glm::vec4(1.f, 0.f, 0.f, -1.f));
	// 90 degrees about z: x goes to y
	glm::mat4 rotation(1.f);
	rotation[0] = glm::vec4(0.f, 1.f, 0.f, 0.f);
	rotation[1] = glm::vec4(-1.f, 0.f, 0.f, 0.f);
	// Non-uniform scale as the doodad palette applies it (128 times the doodad's own scale)
	const glm::mat4 transform = rotation * glm::mat4(glm::vec4(128.f, 0.f, 0.f, 0.f), glm::vec4(0.f, 256.f, 0.f, 0.f),
													 glm::vec4(0.f, 0.f, 128.f, 0.f), glm::vec4(0.f, 0.f, 0.f, 1.f));

	const mdx::MDX result = reread(mdx::merged_for_export({ { &source, transform } }));
	REQUIRE_EQ(result.geosets.size(), 1);
	const auto& g = result.geosets[0];
	REQUIRE_FALSE(g.normals.empty());
	CHECK(g.normals[0].x == doctest::Approx(0.f));
	CHECK(g.normals[0].y == doctest::Approx(1.f));
	REQUIRE_FALSE(g.tangents.empty());
	CHECK(g.tangents[0].x == doctest::Approx(0.f));
	CHECK(g.tangents[0].y == doctest::Approx(1.f));
	CHECK(g.tangents[0].w == doctest::Approx(-1.f));
}

TEST_CASE("merge export keeps each geoset animation on its own model's geoset") {
	mdx::MDX animated = tree();
	mdx::GeosetAnimation animation {};
	animation.alpha = 0.5f;
	animation.geoset_id = 0;
	animated.animations.push_back(animation);
	const mdx::MDX moved = animated;

	const mdx::MDX result = reread(mdx::merged_for_export({
		{ &animated, glm::mat4(1.f) },
		{ &moved, translate(glm::vec3(50.f, 0.f, 0.f)) },
	}));

	check_well_formed(result);
	REQUIRE_EQ(result.animations.size(), 2);
	REQUIRE_EQ(result.geosets.size(), 2);
	CHECK_NE(result.animations[0].geoset_id, result.animations[1].geoset_id);
	const auto& second = result.geosets.at(result.animations[1].geoset_id);
	CHECK(draws_triangle(second, glm::vec3(50.f, 0.f, 0.f), glm::vec3(51.f, 0.f, 0.f), glm::vec3(50.f, 1.f, 0.f)));
	CHECK_EQ(second.vertices.size(), 6);

	// The one bone every vertex is skinned to must not follow either model's geoset animation
	REQUIRE_EQ(result.bones.size(), 1);
	CHECK_EQ(result.bones[0].geoset_animation_id, -1);
	CHECK_EQ(result.bones[0].geoset_id, -1);
}

TEST_CASE("merge export keeps geosets apart when their vertices no longer fit 16-bit face indices") {
	// 2 * 11000 triangles = 66000 vertices, past the 65536 a u16 face index can address
	const mdx::MDX big = model({ "tree.blp" }, { material(0) }, { geoset(0, 11000) });
	const mdx::MDX result = reread(mdx::merged_for_export({ { &big, glm::mat4(1.f) }, { &big, translate(glm::vec3(0.f, 0.f, 9.f)) } }));

	check_well_formed(result);
	REQUIRE_EQ(result.geosets.size(), 2);
	CHECK_EQ(result.geosets[0].vertices.size(), 33000);
	CHECK_EQ(result.geosets[1].vertices.size(), 33000);
	CHECK(draws_triangle(result.geosets[1], glm::vec3(10999.f, 0.f, 9.f), glm::vec3(11000.f, 0.f, 9.f), glm::vec3(10999.f, 1.f, 9.f)));
}

TEST_CASE("merge export keeps geosets apart when uv sets, tangents or lod differ") {
	mdx::MDX two_uv = tree();
	two_uv.geosets[0].uv_sets.push_back(two_uv.geosets[0].uv_sets[0]);
	mdx::MDX tangents = tree();
	tangents.geosets[0].tangents.assign(tangents.geosets[0].vertices.size(), glm::vec4(1.f, 0.f, 0.f, 1.f));
	mdx::MDX lod = tree();
	lod.geosets[0].lod = 1;
	const mdx::MDX plain = tree();

	const mdx::MDX result = reread(mdx::merged_for_export({
		{ &plain, glm::mat4(1.f) },
		{ &two_uv, glm::mat4(1.f) },
		{ &tangents, glm::mat4(1.f) },
		{ &lod, glm::mat4(1.f) },
	}));

	check_well_formed(result);
	CHECK_EQ(result.materials.size(), 1);
	CHECK_EQ(result.geosets.size(), 4);
}

TEST_CASE("deduplicate_geosets merges by material id when ids have gaps") {
	mdx::MDX m = model({ "t0" }, { material(0), material(0), material(0) }, { geoset(2, 1), geoset(2, 1, glm::vec3(0.f, 0.f, 3.f)), geoset(0, 1) });
	m.deduplicate_geosets();
	check_well_formed(m);
	REQUIRE_EQ(m.geosets.size(), 2);
	CHECK_EQ(m.geosets[0].material_id, 2);
	CHECK_EQ(m.geosets[0].vertices.size(), 6);
	CHECK(draws_triangle(m.geosets[0], glm::vec3(0.f, 0.f, 3.f), glm::vec3(1.f, 0.f, 3.f), glm::vec3(0.f, 1.f, 3.f)));
	CHECK_EQ(m.geosets[1].material_id, 0);
}

TEST_CASE("deduplicate_geosets renumbers bone, geoset animation and glider geoset ids") {
	mdx::MDX m = model({ "t0", "t1" }, { material(0), material(1) }, { geoset(0, 1), geoset(1, 1), geoset(0, 1), geoset(1, 1) });
	mdx::GeosetAnimation animation {};
	animation.geoset_id = 3;
	m.animations.push_back(animation);
	m.gliders = { 2, 3, 9 };
	m.bones.emplace_back().geoset_id = 3;
	m.bones.emplace_back().geoset_id = -1;

	m.deduplicate_geosets();

	// Geoset 3 carries an animation, so it stays on its own; geoset 2 joins geoset 0
	REQUIRE_EQ(m.geosets.size(), 3);
	CHECK_EQ(m.animations[0].geoset_id, 2);
	CHECK(m.gliders == std::vector<uint32_t> { 0, 2, 9 });
	CHECK_EQ(m.bones[0].geoset_id, 2);
	CHECK_EQ(m.bones[1].geoset_id, -1);
}

TEST_CASE("merge export with damaged ids keeps them away from other models' data") {
	mdx::MDX bad_material = tree();
	bad_material.geosets[0].material_id = 5;
	mdx::MDX bad_face = tree();
	bad_face.geosets[0].faces[1] = 40;
	mdx::MDX bad_animation = tree();
	mdx::GeosetAnimation animation {};
	animation.geoset_id = 4;
	bad_animation.animations.push_back(animation);
	const mdx::MDX the_rock = rock();
	const mdx::MDX plain = tree();

	mdx::MDX merged = mdx::merged_for_export({
		{ &plain, glm::mat4(1.f) },
		{ &bad_material, glm::mat4(1.f) },
		{ &bad_face, glm::mat4(1.f) },
		{ &bad_animation, glm::mat4(1.f) },
		{ &the_rock, glm::mat4(1.f) },
	});

	// The bad material id must not land on the rock's material
	size_t outside_material = 0;
	for (const auto& g : merged.geosets) {
		if (g.material_id >= merged.materials.size()) {
			outside_material += 1;
		}
	}
	CHECK_EQ(outside_material, 1);
	// The geoset with a face past its vertices is kept on its own, not merged into another model's geometry
	size_t holding_bad_face = 0;
	for (const auto& g : merged.geosets) {
		if (std::ranges::any_of(g.faces, [&](const uint16_t f) { return f >= g.vertices.size(); })) {
			holding_bad_face += 1;
			CHECK_EQ(g.vertices.size(), 6);
		}
	}
	CHECK_EQ(holding_bad_face, 1);
	CHECK(merged.animations.empty());
	CHECK_NOTHROW((void)reread(merged));
}

TEST_CASE("merge export of empty models writes a readable file") {
	const mdx::MDX empty {};
	mdx::MDX result;
	REQUIRE_NOTHROW(result = reread(mdx::merged_for_export({ { &empty, glm::mat4(1.f) }, { nullptr, glm::mat4(1.f) } })));
	CHECK(result.geosets.empty());
	REQUIRE_NOTHROW(result = reread(mdx::merged_for_export({})));
	CHECK(result.geosets.empty());

	const mdx::MDX one = tree();
	REQUIRE_NOTHROW(result = reread(mdx::merged_for_export({ { &empty, glm::mat4(1.f) }, { &one, glm::mat4(1.f) } })));
	check_well_formed(result);
	CHECK_EQ(result.geosets.size(), 1);
}

namespace {
	template <typename T>
	mdx::TrackHeader<T> keys(const std::vector<std::pair<int32_t, T>>& frames, const int32_t global_sequence = -1,
							 const mdx::InterpolationType type = mdx::InterpolationType::linear) {
		mdx::TrackHeader<T> header;
		header.interpolation_type = type;
		header.global_sequence_ID = global_sequence;
		for (const auto& [frame, value] : frames) {
			header.tracks.push_back(mdx::Track<T> { .frame = frame, .value = value, .inTan = value, .outTan = value });
		}
		return header;
	}

	/// Every global sequence id of a track that reaches the merged file, -1 included.
	std::vector<int32_t> global_sequence_ids(mdx::MDX& m) {
		std::vector<int32_t> ids;
		for (auto& animation : m.animations) {
			ids.push_back(animation.KGAO.global_sequence_ID);
			ids.push_back(animation.KGAC.global_sequence_ID);
		}
		for (auto& material : m.materials) {
			for (auto& layer : material.layers) {
				for (const int32_t id : { layer.KMTA.global_sequence_ID, layer.KMTE.global_sequence_ID, layer.KFC3.global_sequence_ID,
										  layer.KFCA.global_sequence_ID, layer.KFTC.global_sequence_ID }) {
					ids.push_back(id);
				}
				for (const auto& slot : layer.textures) {
					ids.push_back(slot.KMTF.global_sequence_ID);
				}
			}
		}
		for (auto& animation : m.texture_animations) {
			ids.push_back(animation.KTAT.global_sequence_ID);
			ids.push_back(animation.KTAR.global_sequence_ID);
			ids.push_back(animation.KTAS.global_sequence_ID);
		}
		return ids;
	}

	/// A water model as NCOW's waterfall: UVs scrolled by KTAT on global sequences 0 and 1, layer alpha on global sequence 2.
	mdx::MDX water() {
		mdx::Material scrolling = material(0, 2);
		scrolling.layers[0].texture_animation_id = 0;
		scrolling.layers[0].KMTA = keys<float>({ { 0, 1.f }, { 5, 0.5f } }, 2);
		mdx::MDX m = model({ "water.blp" }, { scrolling }, { geoset(0, 1) });
		m.global_sequences = { 4000, 9000, 10 };
		m.texture_animations.emplace_back().KTAT = keys<glm::vec3>({ { 0, glm::vec3(0.f) }, { 4000, glm::vec3(1.f, 0.f, 0.f) } }, 0);
		m.texture_animations.emplace_back().KTAT = keys<glm::vec3>({ { 0, glm::vec3(0.f) }, { 9000, glm::vec3(0.f, 1.f, 0.f) } }, 1);
		return m;
	}
} // namespace

TEST_CASE("merge export keeps global sequences and points every track at its own model's ones") {
	const mdx::MDX first = water();
	const mdx::MDX second = water();
	// A damaged id (7 of 3) and an id in a model without global sequences must not name the water's global sequences
	mdx::MDX damaged = water();
	damaged.texture_animations[1].KTAT.global_sequence_ID = 7;
	mdx::MDX without = tree();
	without.materials[0].layers[0].texture_animation_id = 0;
	without.texture_animations.emplace_back().KTAT = keys<glm::vec3>({ { 0, glm::vec3(0.5f, 0.f, 0.f) } }, 1);

	mdx::MDX result = reread(mdx::merged_for_export({
		{ &first, glm::mat4(1.f) },
		{ &second, glm::mat4(1.f) },
		{ &damaged, glm::mat4(1.f) },
		{ &without, glm::mat4(1.f) },
	}));

	check_well_formed(result);
	CHECK(result.global_sequences == std::vector<uint32_t> { 4000, 9000, 10, 4000, 9000, 10, 4000, 9000, 10 });
	for (const int32_t id : global_sequence_ids(result)) {
		CHECK((id == -1 || (id >= 0 && static_cast<size_t>(id) < result.global_sequences.size())));
	}
	// The damaged track and the one without global sequences read their defaults (identity), so those entries are gone
	REQUIRE_EQ(result.texture_animations.size(), 5);
	CHECK_EQ(result.texture_animations[0].KTAT.global_sequence_ID, 0);
	CHECK_EQ(result.texture_animations[1].KTAT.global_sequence_ID, 1);
	CHECK_EQ(result.texture_animations[2].KTAT.global_sequence_ID, 3);
	CHECK_EQ(result.texture_animations[3].KTAT.global_sequence_ID, 4);
	CHECK_EQ(result.texture_animations[4].KTAT.global_sequence_ID, 6);
	CHECK_EQ(result.materials.back().layers[0].texture_animation_id, 0xFFFFFFFFu);
	// Each water layer points at its own texture animation and global sequence, so the two copies stay two materials
	REQUIRE_GE(result.materials.size(), 3);
	CHECK_EQ(result.materials[0].layers[0].KMTA.global_sequence_ID, 2);
	CHECK_EQ(result.materials[1].layers[0].KMTA.global_sequence_ID, 5);
	CHECK_EQ(result.materials[1].layers[0].texture_animation_id, 2);
}

TEST_CASE("deduplicate_materials keeps materials apart that differ only in a track's global sequence") {
	mdx::Material a = material(0);
	a.layers[0].KMTA = keys<float>({ { 0, 1.f }, { 5, 0.f } }, 0);
	mdx::Material b = a;
	b.layers[0].KMTA.global_sequence_ID = 1;
	mdx::MDX m = model({ "t0" }, { a, b, a }, { geoset(0, 1), geoset(1, 1), geoset(2, 1) });
	m.deduplicate_materials();
	REQUIRE_EQ(m.materials.size(), 2);
	CHECK_EQ(m.geosets[1].material_id, 1);
	CHECK_EQ(m.geosets[2].material_id, 0);
}

TEST_CASE("merge export shows each model as at the start of its Stand sequence") {
	// A tree: geoset 0 is the crown (shown in Stand), geoset 1 the stump (alpha 0 in Stand, shown in Death)
	mdx::Material leaves = material(0);
	leaves.layers[0].KMTA = keys<float>({ { 0, 0.f }, { 200, 0.5f } });                                        // 0.5 at Stand start
	leaves.layers[0].textures[0].KMTF = keys<uint32_t>({ { 200, 1u } }, -1, mdx::InterpolationType::none); // texture 1 in Stand
	mdx::MDX source = model({ "bark.blp", "leaves.blp" }, { leaves }, { geoset(0, 1), geoset(0, 1, glm::vec3(0.f, 0.f, 7.f)) });
	source.sequences = {
		mdx::Sequence { .name = "Death", .start_frame = 0, .end_frame = 100 },
		mdx::Sequence { .name = "Stand Alternate", .start_frame = 200, .end_frame = 300 },
	};
	mdx::GeosetAnimation crown {};
	crown.alpha = 1.f;
	crown.color = glm::vec3(1.f);
	crown.geoset_id = 0;
	crown.KGAO = keys<float>({ { 0, 0.f }, { 200, 1.f } }); // 1 in Stand: ends up a default animation
	mdx::GeosetAnimation stump = crown;
	stump.geoset_id = 1;
	stump.KGAO = keys<float>({ { 0, 1.f }, { 200, 0.f }, { 300, 0.f } }); // 0 in Stand: hidden
	source.animations = { crown, stump };
	const mdx::MDX copy = source;

	mdx::MDX result = reread(mdx::merged_for_export({ { &source, glm::mat4(1.f) }, { &copy, translate(glm::vec3(20.f, 0.f, 0.f)) } }));

	check_well_formed(result);
	// The stumps are gone with their animations, the crowns (default animations removed) merged into one geoset
	CHECK(result.animations.empty());
	REQUIRE_EQ(result.geosets.size(), 1);
	CHECK_EQ(result.geosets[0].vertices.size(), 6);
	for (const auto& vertex : result.geosets[0].vertices) {
		CHECK(vertex.z == doctest::Approx(0.f));
	}
	REQUIRE_EQ(result.materials.size(), 1);
	const auto& layer = result.materials[0].layers[0];
	CHECK(layer.KMTA.tracks.empty());
	CHECK(layer.alpha == doctest::Approx(0.5f));
	CHECK(layer.textures[0].KMTF.tracks.empty());
	CHECK_EQ(result.textures.at(layer.textures[0].id).file_name.string(), "leaves.blp");
}

TEST_CASE("frozen_to_stand reduces sequence texture animation to what Stand start shows and keeps global ones running") {
	mdx::MDX source = water();
	source.sequences = { mdx::Sequence { .name = "Stand", .start_frame = 200, .end_frame = 300 } };
	source.texture_animations.emplace_back().KTAT = keys<glm::vec3>({ { 200, glm::vec3(0.25f, 0.f, 0.f) }, { 300, glm::vec3(1.f) } });
	source.texture_animations.emplace_back().KTAS = keys<glm::vec3>({ { 200, glm::vec3(1.f) }, { 300, glm::vec3(2.f) } });

	const mdx::MDX frozen = mdx::frozen_to_stand(source);

	// The KTAS entry shows the identity at Stand start, so it is removed
	REQUIRE_EQ(frozen.texture_animations.size(), 3);
	CHECK(frozen.texture_animations[0].KTAT == source.texture_animations[0].KTAT); // global sequence 0: still running
	CHECK(frozen.texture_animations[1].KTAT == source.texture_animations[1].KTAT);
	REQUIRE_EQ(frozen.texture_animations[2].KTAT.tracks.size(), 1);
	CHECK_EQ(frozen.texture_animations[2].KTAT.tracks[0].frame, 0);
	CHECK(frozen.texture_animations[2].KTAT.tracks[0].value.x == doctest::Approx(0.25f));
	CHECK_EQ(frozen.materials[0].layers[0].texture_animation_id, 0);
	CHECK(frozen.materials[0].layers[0].KMTA == source.materials[0].layers[0].KMTA);
}
