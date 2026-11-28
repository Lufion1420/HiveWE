#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>
#include <doctest/doctest.h>

import <glm/glm.hpp>;
import MDX;
import BinaryReader;
import BinaryWriter;

// Synthetic v800 MDX files with a layer texture flipbook (KMTF) in the MTLS chunk. The pre-v1100 SD reader once stored the
// layer's texture slot before reading its tracks, so every KMTF of a v800 model was lost on load and on Save to MDX.
// mdx::TrackTag is not exported, so tracks are written by hand.

namespace {
	using Bytes = decltype(BinaryWriter::buffer);

	void push_u32(Bytes& out, const uint32_t value) {
		const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
		out.insert(out.end(), bytes, bytes + 4);
	}

	void push_f32(Bytes& out, const float value) {
		uint32_t bits;
		std::memcpy(&bits, &value, 4);
		push_u32(out, bits);
	}

	void push_tag(Bytes& out, const char* tag) {
		out.insert(out.end(), tag, tag + 4);
	}

	/// Stepped (interpolation none) u32 track, no global sequence.
	void push_kmtf(Bytes& out, const std::vector<std::pair<uint32_t, uint32_t>>& keys) {
		push_tag(out, "KMTF");
		push_u32(out, static_cast<uint32_t>(keys.size()));
		push_u32(out, 0);
		push_u32(out, 0xFFFFFFFF);
		for (const auto& [frame, value] : keys) {
			push_u32(out, frame);
			push_u32(out, value);
		}
	}

	/// Linear float track, no global sequence.
	void push_kmta(Bytes& out, const std::vector<std::pair<uint32_t, float>>& keys) {
		push_tag(out, "KMTA");
		push_u32(out, static_cast<uint32_t>(keys.size()));
		push_u32(out, 1);
		push_u32(out, 0xFFFFFFFF);
		for (const auto& [frame, value] : keys) {
			push_u32(out, frame);
			push_f32(out, value);
		}
	}

	/// One v800 layer: inclusive size, filter mode, shading flags, texture id, texture animation id, coord id, alpha, `tracks`.
	Bytes v800_layer(const uint32_t blend_mode, const uint32_t texture_id, const Bytes& tracks) {
		Bytes body;
		push_u32(body, blend_mode);
		push_u32(body, 0x1);		// unshaded
		push_u32(body, texture_id);
		push_u32(body, 0xFFFFFFFF); // no texture animation
		push_u32(body, 0);			// coord id
		push_f32(body, 1.f);		// alpha
		body.insert(body.end(), tracks.begin(), tracks.end());
		Bytes layer;
		push_u32(layer, static_cast<uint32_t>(4 + body.size()));
		layer.insert(layer.end(), body.begin(), body.end());
		return layer;
	}

	/// MTLS payload: one v800 material (priority plane 3, no flags) holding `layers`.
	Bytes v800_material(const std::vector<Bytes>& layers) {
		Bytes body;
		push_u32(body, 3); // priority plane
		push_u32(body, 0); // flags
		push_tag(body, "LAYS");
		push_u32(body, static_cast<uint32_t>(layers.size()));
		for (const auto& layer : layers) {
			body.insert(body.end(), layer.begin(), layer.end());
		}
		Bytes material;
		push_u32(material, static_cast<uint32_t>(4 + body.size()));
		material.insert(material.end(), body.begin(), body.end());
		return material;
	}

	/// One event object named "Evt01" with one KEVT time (frame 10), size 112. Proves the next chunk still lines up.
	void push_evts_chunk(Bytes& out) {
		push_tag(out, "EVTS");
		push_u32(out, 112);
		push_u32(out, 96);
		const char name[80] = "Evt01";
		out.insert(out.end(), name, name + 80);
		push_u32(out, 2); // object id
		push_u32(out, 0xFFFFFFFF);
		push_u32(out, 0x400);
		push_tag(out, "KEVT");
		push_u32(out, 1);
		push_u32(out, 0xFFFFFFFF);
		push_u32(out, 10);
	}

	Bytes build_model(const Bytes& mtls_payload) {
		Bytes out;
		push_tag(out, "MDLX");
		push_tag(out, "VERS");
		push_u32(out, 4);
		push_u32(out, 800);
		push_tag(out, "MTLS");
		push_u32(out, static_cast<uint32_t>(mtls_payload.size()));
		out.insert(out.end(), mtls_payload.begin(), mtls_payload.end());
		push_evts_chunk(out);
		return out;
	}

	mdx::MDX parse(const Bytes& bytes) {
		BinaryReader reader(bytes);
		return mdx::MDX(reader);
	}

	/// The whole top-level chunk with the given tag (tag, size and payload), empty if absent.
	Bytes chunk_bytes(const Bytes& bytes, const char* tag) {
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

	/// Two layers: a plain None layer, then a Blend layer with a stepped flipbook (27 until 4233, then 22) and an alpha track.
	Bytes flipbook_model() {
		Bytes tracks;
		push_kmtf(tracks, { { 333, 27 }, { 4233, 22 } });
		push_kmta(tracks, { { 333, 1.f }, { 3000, 0.5f } });
		return build_model(v800_material({ v800_layer(0, 1, {}), v800_layer(2, 27, tracks) }));
	}

	void check_flipbook(const mdx::MDX& model) {
		REQUIRE_EQ(model.materials.size(), 1);
		const auto& layers = model.materials[0].layers;
		REQUIRE_EQ(layers.size(), 2);
		REQUIRE_EQ(layers[0].textures.size(), 1);
		CHECK_EQ(layers[0].textures[0].id, 1);
		CHECK(layers[0].textures[0].KMTF.tracks.empty());

		REQUIRE_EQ(layers[1].textures.size(), 1);
		const auto& flipbook = layers[1].textures[0].KMTF;
		CHECK_EQ(layers[1].textures[0].id, 27);
		CHECK(flipbook.interpolation_type == mdx::InterpolationType::none);
		CHECK_EQ(flipbook.global_sequence_ID, -1);
		CHECK(flipbook.id >= 0);
		REQUIRE_EQ(flipbook.tracks.size(), 2);
		CHECK_EQ(flipbook.tracks[0].frame, 333);
		CHECK_EQ(flipbook.tracks[0].value, 27);
		CHECK_EQ(flipbook.tracks[1].frame, 4233);
		CHECK_EQ(flipbook.tracks[1].value, 22);
		CHECK_EQ(layers[1].KMTA.tracks.size(), 2);
	}
} // namespace

TEST_CASE("v800 layer keeps its KMTF flipbook track") {
	mdx::MDX model;
	REQUIRE_NOTHROW(model = parse(flipbook_model()));
	check_flipbook(model);
	REQUIRE_EQ(model.event_objects.size(), 1);
}

TEST_CASE("v800 MDX with a KMTF flipbook survives read write read with an identical MTLS chunk") {
	const Bytes in = flipbook_model();
	mdx::MDX model;
	REQUIRE_NOTHROW(model = parse(in));
	const Bytes out = model.to_mdx(800).buffer;
	CHECK(chunk_bytes(out, "MTLS") == chunk_bytes(in, "MTLS"));

	mdx::MDX reparsed;
	REQUIRE_NOTHROW(reparsed = parse(out));
	check_flipbook(reparsed);
	CHECK(chunk_bytes(reparsed.to_mdx(800).buffer, "MTLS") == chunk_bytes(in, "MTLS"));
}

// Env-gated resave sweep for comparing writer output between builds: HIVEWE_MDX_RESAVE_LIST names a UTF-8 file with one model
// path per line, HIVEWE_MDX_RESAVE_OUT an existing folder. Each model is read and written at its own version (as Save to MDX
// does) to <out>/<line number>.mdx; failures go to <out>/<line number>.err. Without both variables the case does nothing.
TEST_CASE("MDX resave sweep (env-gated)") {
#pragma warning(suppress : 4996) // getenv: read on a single-threaded test.
	const char* list = std::getenv("HIVEWE_MDX_RESAVE_LIST");
#pragma warning(suppress : 4996)
	const char* out = std::getenv("HIVEWE_MDX_RESAVE_OUT");
	if (!list || !out) {
		return;
	}
	std::ifstream paths(std::filesystem::path(list), std::ios::binary);
	REQUIRE(paths);
	std::string line;
	int index = 0;
	int written = 0;
	while (std::getline(paths, line)) {
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		if (line.empty()) {
			continue;
		}
		const std::filesystem::path out_base = std::filesystem::path(out) / std::to_string(index++);
		try {
			std::ifstream file(std::filesystem::path(std::u8string(line.begin(), line.end())), std::ios::binary);
			const Bytes bytes { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
			BinaryReader reader(bytes);
			const mdx::MDX model(reader);
			const Bytes saved = model.to_mdx(model.version).buffer;
			std::ofstream result(out_base.string() + ".mdx", std::ios::binary);
			result.write(reinterpret_cast<const char*>(saved.data()), static_cast<std::streamsize>(saved.size()));
			written++;
		} catch (const std::exception& e) {
			std::ofstream(out_base.string() + ".err") << e.what() << '\n';
		}
	}
	MESSAGE("resaved " << written << " of " << index << " models");
}
