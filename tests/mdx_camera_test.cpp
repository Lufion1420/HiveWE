#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
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

// Synthetic MDX files with a CAMS chunk. Since 1.33/2.0 the first dword of a camera entry is the
// entry size in the low 24 bits and a variant in the top byte (real files: 0x03000078).

namespace {
	using Bytes = decltype(BinaryWriter::buffer);

	constexpr uint32_t camera_fixed_size = 120;

	struct CameraEntryOptions {
		uint32_t word = camera_fixed_size; // first dword of the entry as written to the file
		uint32_t body_extra = 0;			  // extra bytes written after near_clip (variant 1/2 payload)
		std::vector<uint8_t> tail;		  // bytes after the target position (tracks or unknown data)
	};

	void write_camera_entry(BinaryWriter& writer, const CameraEntryOptions& options) {
		writer.write<uint32_t>(options.word);
		writer.write_c_string_padded("Camera01", 80);
		writer.write<float>(1.f); // position
		writer.write<float>(2.f);
		writer.write<float>(3.f);
		writer.write<float>(0.7853f); // field of view
		writer.write<float>(5000.f);  // far clip
		writer.write<float>(8.f);	  // near clip
		for (uint32_t i = 0; i < options.body_extra; i++) {
			writer.write<uint8_t>(0xAB);
		}
		writer.write<float>(4.f); // target position
		writer.write<float>(5.f);
		writer.write<float>(6.f);
		writer.write_vector(options.tail);
	}

	/// One event object named "Evt01" with one KEVT time (frame 10), size 112.
	void write_evts_chunk(BinaryWriter& writer) {
		writer.write_string("EVTS");
		writer.write<uint32_t>(112);
		writer.write<uint32_t>(96); // node inclusive size
		writer.write_c_string_padded("Evt01", 80);
		writer.write<uint32_t>(0);			// object id
		writer.write<uint32_t>(0xFFFFFFFF); // parent id
		writer.write<uint32_t>(0x400);		// flags (event object)
		writer.write_string("KEVT");
		writer.write<uint32_t>(1);
		writer.write<int32_t>(-1);
		writer.write<uint32_t>(10);
	}

	/// MDLX + VERS 1800 + CAMS(one entry, chunk size = actual entry bytes) + EVTS
	Bytes build_model(const CameraEntryOptions& options) {
		BinaryWriter writer;
		writer.write_string("MDLX");
		writer.write_string("VERS");
		writer.write<uint32_t>(4);
		writer.write<uint32_t>(1800);

		BinaryWriter entry;
		write_camera_entry(entry, options);

		writer.write_string("CAMS");
		writer.write<uint32_t>(static_cast<uint32_t>(entry.buffer.size()));
		writer.buffer.insert(writer.buffer.end(), entry.buffer.begin(), entry.buffer.end());

		write_evts_chunk(writer);
		return writer.buffer;
	}

	mdx::MDX parse(const Bytes& bytes) {
		BinaryReader reader(bytes);
		return mdx::MDX(reader);
	}

	void check_camera_fields(const mdx::MDX& model) {
		REQUIRE_EQ(model.cameras.size(), 1);
		const auto& camera = model.cameras[0];
		CHECK_EQ(camera.name, "Camera01");
		CHECK_EQ(camera.position.x, 1.f);
		CHECK_EQ(camera.position.y, 2.f);
		CHECK_EQ(camera.position.z, 3.f);
		CHECK_EQ(camera.field_of_view, 0.7853f);
		CHECK_EQ(camera.far_clip, 5000.f);
		CHECK_EQ(camera.near_clip, 8.f);
		CHECK_EQ(camera.target_position.x, 4.f);
		CHECK_EQ(camera.target_position.y, 5.f);
		CHECK_EQ(camera.target_position.z, 6.f);
	}

	void check_event_object(const mdx::MDX& model) {
		REQUIRE_EQ(model.event_objects.size(), 1);
		CHECK_EQ(model.event_objects[0].node.name, "Evt01");
		REQUIRE_EQ(model.event_objects[0].times.size(), 1);
		CHECK_EQ(model.event_objects[0].times[0], 10);
	}

	/// Returns the first dword after the CAMS chunk size, i.e. the first camera entry word.
	uint32_t first_camera_word(const Bytes& bytes) {
		for (size_t i = 0; i + 12 <= bytes.size(); i++) {
			if (std::memcmp(bytes.data() + i, "CAMS", 4) == 0) {
				uint32_t word;
				std::memcpy(&word, bytes.data() + i + 8, 4);
				return word;
			}
		}
		FAIL("no CAMS chunk in buffer");
		return 0;
	}
} // namespace

TEST_CASE("MDX CAMS: variant 3 camera word (0x03000078) parses and the next chunk is still read") {
	CameraEntryOptions options;
	options.word = 0x03000078;
	const auto bytes = build_model(options);

	mdx::MDX model;
	REQUIRE_NOTHROW(model = parse(bytes));
	CHECK_EQ(model.version, 1800);
	check_camera_fields(model);
	check_event_object(model);
}

TEST_CASE("MDX CAMS: variant 3 round-trips through to_mdx as a plain size word") {
	CameraEntryOptions options;
	options.word = 0x03000078;
	mdx::MDX model = parse(build_model(options));

	const BinaryWriter out = model.to_mdx(model.version);
	const Bytes out_bytes = out.buffer;
	CHECK_EQ(first_camera_word(out_bytes), 0x00000078);

	mdx::MDX reread;
	REQUIRE_NOTHROW(reread = parse(out_bytes));
	check_camera_fields(reread);
	check_event_object(reread);
}

TEST_CASE("MDX CAMS: variant 0 camera word parses") {
	CameraEntryOptions options;
	options.word = 0x00000078;
	mdx::MDX model;
	REQUIRE_NOTHROW(model = parse(build_model(options)));
	check_camera_fields(model);
	check_event_object(model);
}

TEST_CASE("MDX CAMS: unknown track data is skipped to the entry end") {
	CameraEntryOptions options;
	options.tail = {'K', 'C', 'V', 'S', 1, 2, 3, 4, 5, 6, 7, 8};
	options.word = 0x03000000 | (camera_fixed_size + static_cast<uint32_t>(options.tail.size()));
	mdx::MDX model;
	REQUIRE_NOTHROW(model = parse(build_model(options)));
	check_camera_fields(model);
	check_event_object(model);
}

TEST_CASE("MDX CAMS: variants 1 and 2 are refused (layout unverified)") {
	for (const uint32_t variant : {1u, 2u}) {
		CameraEntryOptions options;
		options.body_extra = 12;
		options.word = (variant << 24) | (camera_fixed_size + 12);
		CHECK_THROWS_AS(parse(build_model(options)), std::runtime_error);
	}
}

TEST_CASE("MDX CAMS: entry size beyond the chunk end is refused") {
	CameraEntryOptions options;
	options.word = 0x03000000 | (camera_fixed_size + 64);
	CHECK_THROWS_AS(parse(build_model(options)), std::runtime_error);
}

TEST_CASE("MDX CAMS: entry size below the fixed part is refused") {
	CameraEntryOptions options;
	options.word = 0x03000000 | (camera_fixed_size - 4);
	CHECK_THROWS_AS(parse(build_model(options)), std::runtime_error);
}

TEST_CASE("MDX CAMS: decode_camera_word splits size and variant") {
	const mdx::CameraWord v3 = mdx::decode_camera_word(0x03000078);
	CHECK_EQ(v3.size, 120);
	CHECK_EQ(v3.variant, 3);
	CHECK_EQ(v3.extra, 0);

	const mdx::CameraWord v0 = mdx::decode_camera_word(0x00000090);
	CHECK_EQ(v0.size, 0x90);
	CHECK_EQ(v0.variant, 0);
	CHECK_EQ(v0.extra, 0);

	CHECK_THROWS_AS(mdx::decode_camera_word(0x01000084), std::runtime_error);
	CHECK_THROWS_AS(mdx::decode_camera_word(0x02000084), std::runtime_error);
	CHECK_THROWS_AS(mdx::decode_camera_word(0x04000078), std::runtime_error);
	CHECK_THROWS_AS(mdx::decode_camera_word(0xFF000078), std::runtime_error);
}

TEST_CASE("MDX CAMS: unknown variant 4 is refused") {
	CameraEntryOptions options;
	options.word = 0x04000078;
	CHECK_THROWS_AS(parse(build_model(options)), std::runtime_error);
}

namespace {
	void push_u32(std::vector<uint8_t>& out, const uint32_t value) {
		const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
		out.insert(out.end(), bytes, bytes + 4);
	}

	void push_f32(std::vector<uint8_t>& out, const float value) {
		uint32_t bits;
		std::memcpy(&bits, &value, 4);
		push_u32(out, bits);
	}

	/// Track header with linear interpolation and no global sequence; keys are appended by the caller.
	void push_track_header(std::vector<uint8_t>& out, const char* tag, const uint32_t key_count) {
		out.insert(out.end(), tag, tag + 4);
		push_u32(out, key_count);
		push_u32(out, 1);			// linear
		push_u32(out, 0xFFFFFFFF); // no global sequence
	}
} // namespace

TEST_CASE("MDX CAMS: an unknown track before KTTR does not drop KTTR") {
	CameraEntryOptions options;
	push_track_header(options.tail, "KCVS", 2); // unknown float track, 2 keys
	push_u32(options.tail, 0);
	push_f32(options.tail, 0.5f);
	push_u32(options.tail, 100);
	push_f32(options.tail, 1.5f);
	push_track_header(options.tail, "KTTR", 1);
	push_u32(options.tail, 33);
	push_f32(options.tail, 7.f);
	push_f32(options.tail, 8.f);
	push_f32(options.tail, 9.f);
	options.word = 0x03000000 | (camera_fixed_size + static_cast<uint32_t>(options.tail.size()));

	mdx::MDX model;
	REQUIRE_NOTHROW(model = parse(build_model(options)));
	check_camera_fields(model);
	check_event_object(model);
	const auto& kttr = model.cameras[0].KTTR;
	REQUIRE_EQ(kttr.tracks.size(), 1);
	CHECK_EQ(kttr.tracks[0].frame, 33);
	CHECK_EQ(kttr.tracks[0].value.x, 7.f);
	CHECK_EQ(kttr.tracks[0].value.y, 8.f);
	CHECK_EQ(kttr.tracks[0].value.z, 9.f);
}

TEST_CASE("MDX CAMS: track data overrunning its camera entry is refused") {
	CameraEntryOptions options;
	push_track_header(options.tail, "KCTR", 5); // claims 5 keys, only 1 present
	push_u32(options.tail, 0);
	push_f32(options.tail, 1.f);
	push_f32(options.tail, 2.f);
	push_f32(options.tail, 3.f);
	options.word = 0x03000000 | (camera_fixed_size + static_cast<uint32_t>(options.tail.size()));
	CHECK_THROWS_AS(parse(build_model(options)), std::runtime_error);
}

TEST_CASE("MDX CAMS: a CAMS chunk size past the end of the file is refused") {
	CameraEntryOptions options;
	options.word = 0x03000078;
	Bytes bytes = build_model(options);
	REQUIRE(std::memcmp(bytes.data() + 16, "CAMS", 4) == 0);
	const uint32_t too_big = static_cast<uint32_t>(bytes.size());
	std::memcpy(bytes.data() + 20, &too_big, 4);
	CHECK_THROWS_AS(parse(bytes), std::runtime_error);
}

namespace {
	/// Offset of the top-level CAMS chunk tag (walks chunk tags and sizes after MDLX), or 0 if there is none.
	size_t find_cams_chunk(const Bytes& bytes) {
		size_t pos = 4;
		while (pos + 8 <= bytes.size()) {
			uint32_t size;
			std::memcpy(&size, bytes.data() + pos + 4, 4);
			if (std::memcmp(bytes.data() + pos, "CAMS", 4) == 0) {
				REQUIRE(pos + 8 + static_cast<size_t>(size) <= bytes.size());
				return pos;
			}
			pos += 8 + static_cast<size_t>(size);
		}
		return 0;
	}

	template <typename T>
	void check_track_equal(const T& a, const T& b, const char* label) {
		INFO(label);
		CHECK_EQ(a.interpolation_type, b.interpolation_type);
		CHECK_EQ(a.global_sequence_ID, b.global_sequence_ID);
		REQUIRE_EQ(a.tracks.size(), b.tracks.size());
		// inTan/outTan are left uninitialised for none/linear tracks, so compare them only when they are read
		const bool tangents = a.interpolation_type == mdx::InterpolationType::hermite || a.interpolation_type == mdx::InterpolationType::bezier;
		size_t mismatches = 0;
		for (size_t i = 0; i < a.tracks.size(); i++) {
			const auto& x = a.tracks[i];
			const auto& y = b.tracks[i];
			if (x.frame != y.frame || x.value != y.value || (tangents && (x.inTan != y.inTan || x.outTan != y.outTan))) {
				mismatches++;
			}
		}
		CHECK_EQ(mismatches, 0);
	}

	void check_cameras_equal(const mdx::MDX& a, const mdx::MDX& b) {
		REQUIRE_EQ(a.cameras.size(), b.cameras.size());
		for (size_t i = 0; i < a.cameras.size(); i++) {
			const auto& x = a.cameras[i];
			const auto& y = b.cameras[i];
			CHECK_EQ(x.name, y.name);
			CHECK(x.position == y.position);
			CHECK_EQ(x.field_of_view, y.field_of_view);
			CHECK_EQ(x.far_clip, y.far_clip);
			CHECK_EQ(x.near_clip, y.near_clip);
			CHECK(x.target_position == y.target_position);
			check_track_equal(x.KCTR, y.KCTR, "KCTR");
			check_track_equal(x.KCRL, y.KCRL, "KCRL");
			check_track_equal(x.KTTR, y.KTTR, "KTTR");
		}
	}
} // namespace

// Real-model sweep, skipped unless HIVEWE_MDX_DIR names a folder of extracted .mdx files (no Blizzard assets in the repo).
// The CAMS chunk is also parsed on its own (MDLX + the file's VERS + CAMS), so the camera reader is checked even when
// another chunk reader refuses the whole model. Every model must also load whole, and HD skins are checked and resaved.
TEST_CASE("MDX CAMS: stock models in HIVEWE_MDX_DIR parse and round-trip their cameras") {
#pragma warning(suppress : 4996) // getenv: read on a single-threaded test.
	const char* const dir = std::getenv("HIVEWE_MDX_DIR");
	if (dir == nullptr || *dir == '\0') {
		MESSAGE("HIVEWE_MDX_DIR not set, stock MDX camera sweep skipped");
		return;
	}

	size_t files_checked = 0;
	size_t cameras_checked = 0;
	size_t whole_model_failures = 0;
	size_t skinned_models = 0;
	for (const auto& entry : std::filesystem::directory_iterator(dir)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".mdx") {
			continue;
		}
		const std::string file = entry.path().filename().string();
		INFO(file);
		std::ifstream stream(entry.path(), std::ios::binary);
		REQUIRE(stream.is_open());
		const std::vector<char> raw((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		Bytes bytes(raw.size());
		std::memcpy(bytes.data(), raw.data(), raw.size());
		files_checked++;

		bool whole_ok = true;
		mdx::MDX whole;
		try {
			whole = parse(bytes);
		} catch (const std::exception& e) {
			whole_ok = false;
			whole_model_failures++;
			std::printf("%s: whole model refused: %s\n", file.c_str(), e.what());
		}

		// HD skins: 8 entries per vertex, weights summing to 255, unchanged through a same-version save.
		if (whole_ok) {
			bool any_skin = false;
			for (const auto& geoset : whole.geosets) {
				if (geoset.skin.empty()) {
					continue;
				}
				any_skin = true;
				REQUIRE_EQ(geoset.skin.size(), geoset.vertices.size() * 8);
				size_t bad_sums = 0;
				for (size_t v = 0; v < geoset.vertices.size(); v++) {
					const auto* w = geoset.skin.data() + v * 8 + 4;
					if (w[0] + w[1] + w[2] + w[3] != 255) {
						bad_sums++;
					}
				}
				CHECK_EQ(bad_sums, 0);
			}
			if (any_skin) {
				const BinaryWriter resaved = whole.to_mdx(whole.version);
				mdx::MDX reread;
				REQUIRE_NOTHROW(reread = parse(resaved.buffer));
				REQUIRE_EQ(reread.geosets.size(), whole.geosets.size());
				for (size_t g = 0; g < whole.geosets.size(); g++) {
					CHECK(reread.geosets[g].skin == whole.geosets[g].skin);
				}
				skinned_models++;
			}
		}

		const size_t cams = find_cams_chunk(bytes);
		if (cams == 0) {
			std::printf("%s: no CAMS chunk%s\n", file.c_str(), whole_ok ? ", whole model ok" : "");
			continue;
		}
		REQUIRE(bytes.size() >= 16);
		REQUIRE(std::memcmp(bytes.data() + 4, "VERS", 4) == 0);
		uint32_t cams_size;
		std::memcpy(&cams_size, bytes.data() + cams + 4, 4);
		REQUIRE(cams + 8 + static_cast<size_t>(cams_size) <= bytes.size());
		Bytes cams_only(16 + 8 + static_cast<size_t>(cams_size));
		std::memcpy(cams_only.data(), bytes.data(), 16);
		std::memcpy(cams_only.data() + 16, bytes.data() + cams, 8 + static_cast<size_t>(cams_size));

		mdx::MDX model;
		REQUIRE_NOTHROW(model = parse(cams_only));
		REQUIRE_FALSE(model.cameras.empty());
		if (whole_ok) {
			check_cameras_equal(whole, model);
		}
		for (const auto& camera : model.cameras) {
			std::printf("%s: version %u, camera '%s', KCTR %zu KCRL %zu KTTR %zu keys, whole model %s\n", file.c_str(), model.version,
				camera.name.c_str(), camera.KCTR.tracks.size(), camera.KCRL.tracks.size(), camera.KTTR.tracks.size(),
				whole_ok ? "ok" : "refused");
		}

		const BinaryWriter out = model.to_mdx(model.version);
		mdx::MDX reread;
		REQUIRE_NOTHROW(reread = parse(out.buffer));
		check_cameras_equal(model, reread);
		cameras_checked += model.cameras.size();
	}
	std::printf("stock MDX files checked: %zu, cameras round-tripped: %zu, whole models refused: %zu, skinned (HD) models checked: %zu\n",
		files_checked, cameras_checked, whole_model_failures, skinned_models);
	CHECK(files_checked > 0);
	CHECK_EQ(whole_model_failures, 0);
}
