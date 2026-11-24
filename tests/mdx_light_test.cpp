#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <doctest/doctest.h>

import <glm/glm.hpp>;
import MDX;
import BinaryReader;
import BinaryWriter;

// Synthetic MDX files with a LITE chunk (version-dependent light layout) and a DILG chunk (u32 geoset id list).
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

	/// Linear track, no global sequence, one key per (frame, value).
	void push_float_track(Bytes& out, const char* tag, const std::vector<std::pair<uint32_t, float>>& keys) {
		push_tag(out, tag);
		push_u32(out, static_cast<uint32_t>(keys.size()));
		push_u32(out, 1);
		push_u32(out, 0xFFFFFFFF);
		for (const auto& [frame, value] : keys) {
			push_u32(out, frame);
			push_f32(out, value);
		}
	}

	void push_u32_track(Bytes& out, const char* tag, const uint32_t frame, const uint32_t value) {
		push_tag(out, tag);
		push_u32(out, 1);
		push_u32(out, 1);
		push_u32(out, 0xFFFFFFFF);
		push_u32(out, frame);
		push_u32(out, value);
	}

	void push_vec3_track(Bytes& out, const char* tag, const uint32_t frame) {
		push_tag(out, tag);
		push_u32(out, 1);
		push_u32(out, 1);
		push_u32(out, 0xFFFFFFFF);
		push_u32(out, frame);
		push_f32(out, 1.f);
		push_f32(out, 2.f);
		push_f32(out, 3.f);
	}

	/// One light entry in the layout of `version`: inclusive size, node, fixed fields, then `tracks` verbatim.
	Bytes light_entry(const uint32_t version, const Bytes& tracks, const float attenuation_start = 80.f) {
		Bytes body;
		// Node (no node tracks): inclusive size 96
		push_u32(body, 96);
		const char name[80] = "Light01";
		body.insert(body.end(), name, name + 80);
		// Object id 1 and the event object's id 2 survive MDX::validate, which adds a bone with id 2 and compacts the ids
		push_u32(body, 1);			// object id
		push_u32(body, 0xFFFFFFFF); // parent id
		push_u32(body, 0x200);		// flags: light

		push_u32(body, 0); // type omni
		if (version >= 1300) {
			push_u32(body, 1); // shadow casting
		}
		push_f32(body, attenuation_start);
		push_f32(body, 200.f); // attenuation end
		push_f32(body, 1.f);   // color
		push_f32(body, 0.5f);
		push_f32(body, 0.25f);
		push_f32(body, 2.f); // intensity
		push_f32(body, 0.1f); // ambient color
		push_f32(body, 0.2f);
		push_f32(body, 0.3f);
		push_f32(body, 0.5f); // ambient intensity
		if (version >= 1200) {
			push_f32(body, 0.4f); // shadow intensity
		}
		if (version >= 1300) {
			push_f32(body, 10.f); // shadow casting start
			push_f32(body, 20.f); // shadow casting end
		}
		if (version >= 1600) {
			push_f32(body, 9e-5f);	 // quadratic falloff
			push_f32(body, 0.001f); // linear falloff
			push_f32(body, 2e-5f);	 // damping
		}
		body.insert(body.end(), tracks.begin(), tracks.end());

		Bytes entry;
		push_u32(entry, static_cast<uint32_t>(4 + body.size()));
		entry.insert(entry.end(), body.begin(), body.end());
		return entry;
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

	/// MDLX + VERS + one chunk with the given tag and payload + EVTS.
	Bytes build_model(const uint32_t version, const char* tag, const Bytes& payload) {
		Bytes out;
		push_tag(out, "MDLX");
		push_tag(out, "VERS");
		push_u32(out, 4);
		push_u32(out, version);
		push_tag(out, tag);
		push_u32(out, static_cast<uint32_t>(payload.size()));
		out.insert(out.end(), payload.begin(), payload.end());
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

	void check_event_object(const mdx::MDX& model) {
		REQUIRE_EQ(model.event_objects.size(), 1);
		CHECK_EQ(model.event_objects[0].node.name, "Evt01");
	}

	void check_common_light_fields(const mdx::MDX& model, const float attenuation_start = 80.f) {
		REQUIRE_EQ(model.lights.size(), 1);
		const auto& light = model.lights[0];
		CHECK_EQ(light.node.name, "Light01");
		CHECK_EQ(light.type, 0);
		CHECK_EQ(light.attenuation_start, attenuation_start);
		CHECK_EQ(light.attenuation_end, 200.f);
		CHECK(light.color == glm::vec3(1.f, 0.5f, 0.25f));
		CHECK_EQ(light.intensity, 2.f);
		CHECK(light.ambient_color == glm::vec3(0.1f, 0.2f, 0.3f));
		CHECK_EQ(light.ambient_intensity, 0.5f);
		CHECK_EQ(light.shadow_intensity, 0.4f);
	}

	/// Parses a single-light model, checks the LITE chunk is byte-identical after a same-version resave.
	mdx::MDX check_lite_roundtrip(const uint32_t version, const Bytes& tracks, const float attenuation_start = 80.f) {
		const Bytes in = build_model(version, "LITE", light_entry(version, tracks, attenuation_start));
		mdx::MDX model;
		REQUIRE_NOTHROW(model = parse(in));
		check_event_object(model);
		check_common_light_fields(model, attenuation_start);
		const Bytes out = model.to_mdx(version).buffer;
		CHECK(chunk_bytes(out, "LITE") == chunk_bytes(in, "LITE"));
		return model;
	}

	/// KLAS, KLAC, KLAV, all valid at every version
	Bytes classic_tracks() {
		Bytes tracks;
		push_u32_track(tracks, "KLAS", 0, 75);
		push_vec3_track(tracks, "KLAC", 5);
		push_float_track(tracks, "KLAV", {{0, 1.f}, {100, 0.f}});
		return tracks;
	}
} // namespace

TEST_CASE("MDX LITE: a v1800 light with a KLLF track is byte-identical after a resave") {
	Bytes tracks = classic_tracks();
	push_float_track(tracks, "KLLF", {{0, 0.5f}, {50, 0.75f}});
	const mdx::MDX model = check_lite_roundtrip(1800, tracks);
	const auto& light = model.lights[0];
	CHECK_EQ(light.shadow_casting, 1);
	CHECK_EQ(light.shadow_casting_start, 10.f);
	CHECK_EQ(light.shadow_casting_end, 20.f);
	CHECK_EQ(light.quadratic_falloff, 9e-5f);
	CHECK_EQ(light.linear_falloff, 0.001f);
	CHECK_EQ(light.damping, 2e-5f);
	REQUIRE_EQ(light.KLAS.tracks.size(), 1);
	CHECK_EQ(light.KLAS.tracks[0].value, 75);
	CHECK_EQ(light.KLAV.tracks.size(), 2);
	REQUIRE_EQ(light.KLLF.tracks.size(), 2);
	CHECK_EQ(light.KLLF.tracks[1].frame, 50);
	CHECK_EQ(light.KLLF.tracks[1].value, 0.75f);
	CHECK(light.raw_tracks.empty());
}

TEST_CASE("MDX LITE: a v1300 light is byte-identical after a resave and keeps the v1600 defaults") {
	Bytes tracks = classic_tracks();
	push_float_track(tracks, "KLSE", {{0, 30.f}});
	const mdx::MDX model = check_lite_roundtrip(1300, tracks);
	const auto& light = model.lights[0];
	CHECK_EQ(light.shadow_casting, 1);
	CHECK_EQ(light.shadow_casting_start, 10.f);
	CHECK_EQ(light.shadow_casting_end, 20.f);
	CHECK_EQ(light.quadratic_falloff, 0.0005f);
	CHECK_EQ(light.linear_falloff, 0.f);
	CHECK_EQ(light.damping, 0.00001f);
	CHECK_EQ(light.KLAC.tracks.size(), 1);
	CHECK_EQ(light.KLSE.tracks.size(), 1);
}

TEST_CASE("MDX LITE: a v1200 light is byte-identical after a resave") {
	const mdx::MDX model = check_lite_roundtrip(1200, classic_tracks());
	const auto& light = model.lights[0];
	CHECK_EQ(light.shadow_casting, 0);
	CHECK_EQ(light.quadratic_falloff, 0.0005f);
	CHECK_EQ(light.damping, 0.00001f);
	CHECK_EQ(light.KLAV.tracks.size(), 2);
}

TEST_CASE("MDX LITE: an unknown light track is kept verbatim and the next chunk still lines up") {
	Bytes tracks = classic_tracks();
	Bytes unknown;
	push_float_track(unknown, "KLZZ", {{0, 1.f}});
	tracks.insert(tracks.end(), unknown.begin(), unknown.end());
	const mdx::MDX model = check_lite_roundtrip(1800, tracks);
	CHECK(std::equal(model.lights[0].raw_tracks.begin(), model.lights[0].raw_tracks.end(), unknown.begin(), unknown.end()));

	// Dropped when written in an older layout
	const Bytes out = model.to_mdx(1200).buffer;
	const Bytes expected = build_model(1200, "LITE", light_entry(1200, classic_tracks()));
	CHECK(chunk_bytes(out, "LITE") == chunk_bytes(expected, "LITE"));
}

TEST_CASE("MDX LITE: writing a v1800 light at v1200 drops the newer fields and tracks") {
	Bytes tracks_1800 = classic_tracks();
	push_float_track(tracks_1800, "KLSS", {{0, 5.f}});
	push_float_track(tracks_1800, "KLLF", {{0, 0.5f}});
	const mdx::MDX model = parse(build_model(1800, "LITE", light_entry(1800, tracks_1800)));

	const Bytes out = model.to_mdx(1200).buffer;
	const Bytes expected = build_model(1200, "LITE", light_entry(1200, classic_tracks()));
	const Bytes lite = chunk_bytes(out, "LITE");
	REQUIRE_FALSE(lite.empty());
	CHECK(lite == chunk_bytes(expected, "LITE"));
	uint32_t chunk_size;
	uint32_t entry_size;
	std::memcpy(&chunk_size, lite.data() + 4, 4);
	std::memcpy(&entry_size, lite.data() + 8, 4);
	// size dword, node, 12 fixed dwords (type .. shadow intensity), tracks
	CHECK_EQ(entry_size, 4 + 96 + 12 * 4 + static_cast<uint32_t>(classic_tracks().size()));
	CHECK_EQ(chunk_size, entry_size);

	mdx::MDX reread;
	REQUIRE_NOTHROW(reread = parse(out));
	check_common_light_fields(reread);
}

TEST_CASE("MDX LITE: a fractional attenuation survives a resave") {
	const mdx::MDX model = check_lite_roundtrip(1800, classic_tracks(), 80.5f);
	CHECK_EQ(model.lights[0].attenuation_start, 80.5f);
}

TEST_CASE("MDX LITE: a v800 light (no shadow intensity) is byte-identical after a resave") {
	const mdx::MDX model = check_lite_roundtrip(800, classic_tracks());
	CHECK_EQ(model.lights[0].shadow_casting, 0);
	CHECK_EQ(model.lights[0].quadratic_falloff, 0.0005f);
}

TEST_CASE("MDX LITE: the v1600 boundary: v1500 has no falloff fields, v1600 has them") {
	{
		const mdx::MDX model = check_lite_roundtrip(1500, classic_tracks());
		const auto& light = model.lights[0];
		CHECK_EQ(light.shadow_casting, 1);
		CHECK_EQ(light.shadow_casting_end, 20.f);
		CHECK_EQ(light.quadratic_falloff, 0.0005f);
		CHECK_EQ(light.linear_falloff, 0.f);
		CHECK_EQ(light.damping, 0.00001f);
	}
	{
		Bytes tracks = classic_tracks();
		push_float_track(tracks, "KLDA", {{0, 3e-5f}});
		const mdx::MDX model = check_lite_roundtrip(1600, tracks);
		const auto& light = model.lights[0];
		CHECK_EQ(light.quadratic_falloff, 9e-5f);
		CHECK_EQ(light.linear_falloff, 0.001f);
		CHECK_EQ(light.damping, 2e-5f);
		CHECK_EQ(light.KLDA.tracks.size(), 1);
		CHECK(light.raw_tracks.empty());
	}
}

TEST_CASE("MDX LITE: tracks newer than the file version are kept raw and written back") {
	{
		Bytes newer;
		push_float_track(newer, "KLSS", {{0, 5.f}});
		push_float_track(newer, "KLQF", {{0, 1e-4f}});
		Bytes tracks = classic_tracks();
		tracks.insert(tracks.end(), newer.begin(), newer.end());
		const mdx::MDX model = check_lite_roundtrip(1200, tracks);
		const auto& light = model.lights[0];
		CHECK(light.KLSS.tracks.empty());
		CHECK(light.KLQF.tracks.empty());
		CHECK(std::equal(light.raw_tracks.begin(), light.raw_tracks.end(), newer.begin(), newer.end()));
		CHECK_EQ(light.KLAV.tracks.size(), 2);
	}
	{
		Bytes newer;
		push_float_track(newer, "KLLF", {{0, 0.5f}});
		Bytes tracks = classic_tracks();
		push_float_track(tracks, "KLSS", {{0, 5.f}});
		tracks.insert(tracks.end(), newer.begin(), newer.end());
		const mdx::MDX model = check_lite_roundtrip(1300, tracks);
		const auto& light = model.lights[0];
		CHECK_EQ(light.KLSS.tracks.size(), 1);
		CHECK(light.KLLF.tracks.empty());
		CHECK(std::equal(light.raw_tracks.begin(), light.raw_tracks.end(), newer.begin(), newer.end()));
	}
}

TEST_CASE("MDX LITE: a track overrunning its light entry is kept raw and the next chunk still lines up") {
	for (const uint32_t key_count : {5u, 0x10000000u}) {
		Bytes tracks = classic_tracks();
		Bytes bad;
		push_tag(bad, "KLAI");
		push_u32(bad, key_count); // more keys than the entry holds (or the whole file)
		push_u32(bad, 1);
		push_u32(bad, 0xFFFFFFFF);
		push_u32(bad, 0);
		push_f32(bad, 1.f);
		tracks.insert(tracks.end(), bad.begin(), bad.end());
		INFO("key count ", key_count);
		const mdx::MDX model = check_lite_roundtrip(1800, tracks);
		const auto& light = model.lights[0];
		CHECK(light.KLAI.tracks.empty());
		CHECK_EQ(light.KLAV.tracks.size(), 2);
		CHECK(std::equal(light.raw_tracks.begin(), light.raw_tracks.end(), bad.begin(), bad.end()));
	}
}

TEST_CASE("MDX LITE: a light entry size past the end of the LITE chunk is refused") {
	Bytes entry = light_entry(1800, classic_tracks());
	uint32_t size;
	std::memcpy(&size, entry.data(), 4);
	size += 8;
	std::memcpy(entry.data(), &size, 4);
	CHECK_THROWS_AS(parse(build_model(1800, "LITE", entry)), std::runtime_error);
}

TEST_CASE("MDX LITE: a light entry smaller than its fixed fields is refused") {
	Bytes entry = light_entry(1800, {});
	const uint32_t size = 4 + 96 + 8; // size dword, node, two of the fixed dwords
	std::memcpy(entry.data(), &size, 4);
	CHECK_THROWS_AS(parse(build_model(1800, "LITE", entry)), std::runtime_error);
}

namespace {
	Bytes dilg_payload(const std::vector<uint32_t>& ids) {
		Bytes payload;
		for (const uint32_t id : ids) {
			push_u32(payload, id);
		}
		return payload;
	}
} // namespace

TEST_CASE("MDX DILG: geoset id lists are written back last, in file order") {
	for (const std::vector<uint32_t>& ids : {std::vector<uint32_t>{3, 7}, std::vector<uint32_t>(7, 0)}) {
		const Bytes in = build_model(1800, "DILG", dilg_payload(ids));
		mdx::MDX model;
		REQUIRE_NOTHROW(model = parse(in));
		check_event_object(model);

		CHECK(model.gliders == ids);

		const Bytes out = model.to_mdx(1800).buffer;
		const Bytes dilg = chunk_bytes(in, "DILG");
		REQUIRE_FALSE(dilg.empty());
		REQUIRE(out.size() >= dilg.size());
		CHECK(Bytes(out.end() - dilg.size(), out.end()) == dilg);
	}
}

TEST_CASE("MDX DILG: a second DILG chunk appends its entries") {
	Bytes bytes = build_model(1800, "DILG", dilg_payload({3, 7}));
	push_tag(bytes, "DILG");
	push_u32(bytes, 4);
	push_u32(bytes, 9);
	mdx::MDX model;
	REQUIRE_NOTHROW(model = parse(bytes));
	CHECK(model.gliders == std::vector<uint32_t>{3, 7, 9});
}

TEST_CASE("MDX DILG: a model without a DILG chunk writes none") {
	const Bytes out = parse(build_model(1800, "LITE", light_entry(1800, classic_tracks()))).to_mdx(1800).buffer;
	CHECK(chunk_bytes(out, "DILG").empty());
}

TEST_CASE("MDX DILG: a size that is not a multiple of 4 is refused") {
	Bytes payload = dilg_payload({3});
	payload.push_back(0);
	payload.push_back(0);
	CHECK_THROWS_AS(parse(build_model(1800, "DILG", payload)), std::runtime_error);
}

TEST_CASE("MDX DILG: a size past the end of the file is refused") {
	Bytes bytes;
	push_tag(bytes, "MDLX");
	push_tag(bytes, "VERS");
	push_u32(bytes, 4);
	push_u32(bytes, 1800);
	push_tag(bytes, "DILG");
	push_u32(bytes, 64);
	push_u32(bytes, 3);
	CHECK_THROWS_AS(parse(bytes), std::runtime_error);
}

// Real-model sweep, skipped unless HIVEWE_MDX_DIR names a folder of extracted .mdx files (no Blizzard assets in the repo).
// Every model with a LITE or DILG chunk must load, and both chunks must be byte-identical after a same-version resave.
TEST_CASE("MDX LITE/DILG: stock models in HIVEWE_MDX_DIR keep their lights and DILG chunk byte-exact") {
#pragma warning(suppress : 4996) // getenv: read on a single-threaded test.
	const char* const dir = std::getenv("HIVEWE_MDX_DIR");
	if (dir == nullptr || *dir == '\0') {
		MESSAGE("HIVEWE_MDX_DIR not set, stock MDX light/DILG sweep skipped");
		return;
	}

	size_t files_checked = 0;
	size_t lights_checked = 0;
	size_t dilg_checked = 0;
	size_t failures = 0;
	for (const auto& entry : std::filesystem::directory_iterator(dir)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".mdx") {
			continue;
		}
		const std::string file = entry.path().filename().string();
		std::ifstream stream(entry.path(), std::ios::binary);
		REQUIRE(stream.is_open());
		const std::vector<char> raw((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		Bytes bytes(raw.size());
		std::memcpy(bytes.data(), raw.data(), raw.size());
		files_checked++;

		const Bytes lite_in = chunk_bytes(bytes, "LITE");
		const Bytes dilg_in = chunk_bytes(bytes, "DILG");
		if (lite_in.empty() && dilg_in.empty()) {
			continue;
		}
		mdx::MDX model;
		try {
			model = parse(bytes);
		} catch (const std::exception& e) {
			failures++;
			std::printf("%s: refused: %s\n", file.c_str(), e.what());
			continue;
		}
		const Bytes out = model.to_mdx(model.version).buffer;
		if (!lite_in.empty()) {
			lights_checked += model.lights.size();
			if (chunk_bytes(out, "LITE") != lite_in) {
				failures++;
				std::printf("%s: LITE chunk differs after a resave\n", file.c_str());
			}
		}
		if (!dilg_in.empty()) {
			dilg_checked++;
			if (chunk_bytes(out, "DILG") != dilg_in) {
				failures++;
				std::printf("%s: DILG chunk differs after a resave\n", file.c_str());
			}
		}
	}
	std::printf("stock MDX files checked: %zu, lights round-tripped: %zu, DILG chunks: %zu, failures: %zu\n", files_checked,
		lights_checked, dilg_checked, failures);
	CHECK(files_checked > 0);
	CHECK_EQ(failures, 0);
}
