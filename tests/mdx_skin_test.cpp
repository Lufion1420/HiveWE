#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <doctest/doctest.h>

import <glm/glm.hpp>;
import MDX;
import BinaryReader;
import BinaryWriter;

// Synthetic MDX files with one geoset that has a SKIN chunk. From v1400 on the SKIN count is the number of u16
// elements (8 per vertex: 4 bone indices into MATS, then 4 weights summing to 255). Before v1400 it is a byte count
// with the same 8 entries per vertex.

namespace {
	using Bytes = decltype(BinaryWriter::buffer);

	constexpr uint32_t vertex_count = 3;

	/// Skin entries for the 3 vertices: indices then weights. Indices may exceed 255 to test refusal.
	std::vector<uint16_t> sample_skin(const uint16_t first_index = 7) {
		return {
			first_index, 1, 0, 0, 255, 0, 0, 0, //
			2, 3, 4, 5, 100, 80, 50, 25,	   //
			6, 0, 0, 0, 130, 125, 0, 0,		   //
		};
	}

	void write_geoset(BinaryWriter& g, const uint32_t version, const std::vector<uint16_t>& skin) {
		g.write_string("VRTX");
		g.write<uint32_t>(vertex_count);
		g.write(glm::vec3(0.f, 0.f, 0.f));
		g.write(glm::vec3(1.f, 0.f, 0.f));
		g.write(glm::vec3(0.f, 1.f, 0.f));
		g.write_string("NRMS");
		g.write<uint32_t>(vertex_count);
		for (uint32_t i = 0; i < vertex_count; i++) {
			g.write(glm::vec3(0.f, 0.f, 1.f));
		}
		g.write_string("PTYP");
		g.write<uint32_t>(1);
		g.write<uint32_t>(4);
		g.write_string("PCNT");
		g.write<uint32_t>(1);
		g.write<uint32_t>(3);
		g.write_string("PVTX");
		g.write<uint32_t>(3);
		g.write<uint16_t>(0);
		g.write<uint16_t>(1);
		g.write<uint16_t>(2);
		g.write_string("GNDX");
		g.write<uint32_t>(vertex_count);
		for (uint32_t i = 0; i < vertex_count; i++) {
			g.write<uint8_t>(0);
		}
		g.write_string("MTGC");
		g.write<uint32_t>(1);
		g.write<uint32_t>(1);
		g.write_string("MATS");
		g.write<uint32_t>(1);
		g.write<uint32_t>(0);
		g.write<uint32_t>(0); // material id
		g.write<uint32_t>(0); // selection group
		g.write<uint32_t>(0); // selection flags
		g.write<uint32_t>(0); // lod
		g.write_c_string_padded("", 80);
		g.write<float>(1.f); // extent
		g.write(glm::vec3(0.f, 0.f, 0.f));
		g.write(glm::vec3(1.f, 1.f, 0.f));
		g.write<uint32_t>(0); // sequence extents
		g.write_string("TANG");
		g.write<uint32_t>(vertex_count);
		for (uint32_t i = 0; i < vertex_count; i++) {
			g.write(glm::vec4(1.f, 0.f, 0.f, 1.f));
		}
		g.write_string("SKIN");
		if (version >= 1400) {
			g.write<uint32_t>(static_cast<uint32_t>(skin.size()));
			g.write_vector(skin);
		} else {
			g.write<uint32_t>(static_cast<uint32_t>(skin.size()));
			for (const uint16_t value : skin) {
				g.write<uint8_t>(static_cast<uint8_t>(value));
			}
		}
		g.write_string("UVAS");
		g.write<uint32_t>(1);
		g.write_string("UVBS");
		g.write<uint32_t>(vertex_count);
		g.write(glm::vec2(0.f, 0.f));
		g.write(glm::vec2(1.f, 0.f));
		g.write(glm::vec2(0.f, 1.f));
	}

	/// MDLX + VERS + MODL + GEOS (one geoset), in the order MDX::to_mdx writes them.
	Bytes build_model(const uint32_t version, const std::vector<uint16_t>& skin) {
		BinaryWriter writer;
		writer.write_string("MDLX");
		writer.write_string("VERS");
		writer.write<uint32_t>(4);
		writer.write<uint32_t>(version);

		writer.write_string("MODL");
		writer.write<uint32_t>(372);
		writer.write_c_string_padded("SkinTest", 80);
		writer.write_c_string_padded("", 260);
		writer.write<float>(1.f);
		writer.write(glm::vec3(0.f, 0.f, 0.f));
		writer.write(glm::vec3(1.f, 1.f, 0.f));
		writer.write<uint32_t>(150); // blend time

		BinaryWriter geoset;
		write_geoset(geoset, version, skin);

		writer.write_string("GEOS");
		writer.write<uint32_t>(static_cast<uint32_t>(geoset.buffer.size() + 4));
		writer.write<uint32_t>(static_cast<uint32_t>(geoset.buffer.size() + 4)); // inclusive geoset size
		writer.buffer.insert(writer.buffer.end(), geoset.buffer.begin(), geoset.buffer.end());

		// A chunk after GEOS, so a misread SKIN length cannot hide at the end of the file.
		writer.write_string("GLBS");
		writer.write<uint32_t>(4);
		writer.write<uint32_t>(1000);
		return writer.buffer;
	}

	mdx::MDX parse(const Bytes& bytes) {
		BinaryReader reader(bytes);
		return mdx::MDX(reader);
	}

	/// The bytes of a top-level chunk (tag, size and body), or empty if the file has none.
	Bytes chunk(const Bytes& bytes, const char* tag) {
		size_t pos = 4;
		while (pos + 8 <= bytes.size()) {
			uint32_t size;
			std::memcpy(&size, bytes.data() + pos + 4, 4);
			if (std::memcmp(bytes.data() + pos, tag, 4) == 0) {
				REQUIRE(pos + 8 + static_cast<size_t>(size) <= bytes.size());
				return Bytes(bytes.begin() + pos, bytes.begin() + pos + 8 + size);
			}
			pos += 8 + static_cast<size_t>(size);
		}
		return {};
	}

	/// SKIN count dword and the following bytes of the first geoset, located by the "SKIN" tag.
	uint32_t skin_count_in_file(const Bytes& bytes) {
		for (size_t i = 0; i + 8 <= bytes.size(); i++) {
			if (std::memcmp(bytes.data() + i, "SKIN", 4) == 0) {
				uint32_t count;
				std::memcpy(&count, bytes.data() + i + 4, 4);
				return count;
			}
		}
		FAIL("no SKIN tag in buffer");
		return 0;
	}

	void check_skin(const mdx::MDX& model, const std::vector<uint16_t>& expected) {
		REQUIRE_EQ(model.geosets.size(), 1);
		const auto& geoset = model.geosets[0];
		CHECK_EQ(geoset.vertices.size(), vertex_count);
		REQUIRE_EQ(geoset.skin.size(), expected.size());
		for (size_t i = 0; i < expected.size(); i++) {
			INFO("skin entry " << i);
			CHECK_EQ(static_cast<unsigned>(geoset.skin[i]), static_cast<unsigned>(expected[i]));
		}
		REQUIRE_EQ(geoset.uv_sets.size(), 1);
		CHECK_EQ(geoset.uv_sets[0].size(), vertex_count);
		REQUIRE_EQ(model.global_sequences.size(), 1);
		CHECK_EQ(model.global_sequences[0], 1000);
	}
} // namespace

TEST_CASE("MDX SKIN: v1800 SKIN count is u16 elements, 8 per vertex") {
	const auto skin = sample_skin();
	const Bytes bytes = build_model(1800, skin);
	CHECK_EQ(skin_count_in_file(bytes), vertex_count * 8);

	mdx::MDX model;
	REQUIRE_NOTHROW(model = parse(bytes));
	CHECK_EQ(model.version, 1800);
	check_skin(model, skin);
}

TEST_CASE("MDX SKIN: v1800 GEOS round-trips byte-exact") {
	const Bytes bytes = build_model(1800, sample_skin(300));
	const mdx::MDX model = parse(bytes);
	const BinaryWriter out = model.to_mdx(model.version);
	const Bytes in_geos = chunk(bytes, "GEOS");
	REQUIRE_FALSE(in_geos.empty());
	CHECK(chunk(out.buffer, "GEOS") == in_geos);
	check_skin(parse(out.buffer), sample_skin(300));
}

TEST_CASE("MDX SKIN: v1200 SKIN stays one byte per entry and round-trips byte-exact") {
	const auto skin = sample_skin();
	const Bytes bytes = build_model(1200, skin);
	mdx::MDX model;
	REQUIRE_NOTHROW(model = parse(bytes));
	check_skin(model, skin);

	const BinaryWriter out = model.to_mdx(1200);
	const Bytes in_geos = chunk(bytes, "GEOS");
	REQUIRE_FALSE(in_geos.empty());
	CHECK(chunk(out.buffer, "GEOS") == in_geos);
	CHECK_EQ(skin_count_in_file(out.buffer), vertex_count * 8);
}

TEST_CASE("MDX SKIN: converting between v1200 and v1800 changes the entry width, not the values") {
	const auto skin = sample_skin();
	const mdx::MDX from_v1200 = parse(build_model(1200, skin));
	const BinaryWriter as_v1800 = from_v1200.to_mdx(1800);
	CHECK(chunk(as_v1800.buffer, "GEOS") == chunk(build_model(1800, skin), "GEOS"));

	const mdx::MDX from_v1800 = parse(build_model(1800, skin));
	const BinaryWriter as_v1200 = from_v1800.to_mdx(1200);
	CHECK(chunk(as_v1200.buffer, "GEOS") == chunk(build_model(1200, skin), "GEOS"));
}

TEST_CASE("MDX SKIN: a bone index above 255 is refused when writing below v1400") {
	const mdx::MDX model = parse(build_model(1800, sample_skin(300)));
	CHECK_THROWS_AS((void)model.to_mdx(1200), std::runtime_error);
	CHECK_NOTHROW((void)model.to_mdx(1800));
}

TEST_CASE("MDX SKIN: GPU packing keeps one byte per entry and refuses an index above 255") {
	const mdx::MDX model = parse(build_model(1800, sample_skin()));
	REQUIRE(mdx::MDX::skin_fits_u8(model.geosets[0]));
	const auto packed = mdx::MDX::skin_as_u8_weights(model.geosets[0]);
	REQUIRE_EQ(packed.size(), vertex_count * 2);
	CHECK(packed[0] == glm::u8vec4(7, 1, 0, 0));
	CHECK(packed[1] == glm::u8vec4(255, 0, 0, 0));
	CHECK(packed[3] == glm::u8vec4(100, 80, 50, 25));

	const mdx::MDX wide = parse(build_model(1800, sample_skin(300)));
	CHECK_FALSE(mdx::MDX::skin_fits_u8(wide.geosets[0]));
	CHECK_THROWS_AS(mdx::MDX::skin_as_u8_weights(wide.geosets[0]), std::runtime_error);
}

TEST_CASE("MDX SKIN: a v1800 SKIN count past the end of the file is refused") {
	Bytes bytes = build_model(1800, sample_skin());
	for (size_t i = 0; i + 8 <= bytes.size(); i++) {
		if (std::memcmp(bytes.data() + i, "SKIN", 4) == 0) {
			const uint32_t too_big = static_cast<uint32_t>(bytes.size());
			std::memcpy(bytes.data() + i + 4, &too_big, 4);
			break;
		}
	}
	CHECK_THROWS_AS(parse(bytes), std::out_of_range);
}
