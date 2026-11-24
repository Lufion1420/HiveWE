#include <cstdint>
#include <stdexcept>
#include <vector>
#include <doctest/doctest.h>

import <glm/glm.hpp>;
import MDX;
import BinaryReader;
import BinaryWriter;

// TrackHeader reads its track count from the file; a damaged count must fail the read, not allocate from it.

namespace {
	BinaryReader reader_from(const BinaryWriter& writer) {
		return BinaryReader(writer.buffer);
	}
}

TEST_CASE("TrackHeader refuses a forged track count") {
	BinaryWriter writer;
	writer.write<uint32_t>(0x7FFFFFFF); // count
	writer.write<uint32_t>(1);			// linear
	writer.write<int32_t>(-1);			// global sequence
	writer.write<int32_t>(0);			// one frame of data, nowhere near enough

	// The position stays right after the count: the refusal happens before anything else is read.
	BinaryReader reader = reader_from(writer);
	CHECK_THROWS_AS((mdx::TrackHeader<glm::vec3>(reader, 0)), std::out_of_range);
	CHECK(reader.position == 4);

	BinaryWriter writer2;
	writer2.write<uint32_t>(0xFFFFFFFF);
	BinaryReader reader2 = reader_from(writer2);
	CHECK_THROWS_AS((mdx::TrackHeader<float>(reader2, 0)), std::out_of_range);
	CHECK(reader2.position == 4);
}

TEST_CASE("TrackHeader refuses a count larger than the remaining bytes") {
	BinaryWriter writer;
	writer.write<uint32_t>(4); // claims 4 tracks; 28 bytes remain and the smallest float record is 8, so at most 3 fit
	writer.write<uint32_t>(2);
	writer.write<int32_t>(-1);
	for (int i = 0; i < 5; i++) {
		writer.write<uint32_t>(0);
	}

	BinaryReader reader = reader_from(writer);
	CHECK_THROWS_AS((mdx::TrackHeader<float>(reader, 0)), std::out_of_range);
	CHECK(reader.position == 4);
}

TEST_CASE("TrackHeader round-trips a valid header") {
	// TrackTag is not exported, so the file bytes are written by hand (same layout as TrackHeader::save).
	mdx::TrackHeader<float> header;
	header.interpolation_type = mdx::InterpolationType::hermite;
	header.global_sequence_ID = 2;
	BinaryWriter writer;
	writer.write<uint32_t>(3);
	writer.write<uint32_t>(2);
	writer.write<int32_t>(2);
	for (int i = 0; i < 3; i++) {
		mdx::Track<float> track;
		track.frame = i * 100;
		track.value = static_cast<float>(i) + 0.5f;
		track.inTan = 1.f + i;
		track.outTan = 2.f + i;
		header.tracks.push_back(track);
		writer.write<int32_t>(track.frame);
		writer.write<float>(track.value);
		writer.write<float>(track.inTan);
		writer.write<float>(track.outTan);
	}

	BinaryReader reader = reader_from(writer);
	const mdx::TrackHeader<float> read(reader, 7);

	CHECK(reader.remaining() == 0);
	CHECK(read.id == 7);
	CHECK(read.interpolation_type == mdx::InterpolationType::hermite);
	CHECK(read.global_sequence_ID == 2);
	REQUIRE(read.tracks.size() == 3);
	for (size_t i = 0; i < 3; i++) {
		CHECK(read.tracks[i].frame == header.tracks[i].frame);
		CHECK(read.tracks[i].value == header.tracks[i].value);
		CHECK(read.tracks[i].inTan == header.tracks[i].inTan);
		CHECK(read.tracks[i].outTan == header.tracks[i].outTan);
	}
}

TEST_CASE("TrackHeader accepts linear tracks that end exactly at the end of the buffer") {
	// float: 4 byte frame + 4 byte value = 8 bytes per track, the smallest record the count check assumes.
	BinaryWriter writer;
	writer.write<uint32_t>(3);
	writer.write<uint32_t>(1); // linear
	writer.write<int32_t>(-1);
	for (int i = 0; i < 3; i++) {
		writer.write<int32_t>(i * 10);
		writer.write<float>(static_cast<float>(i));
	}

	BinaryReader reader = reader_from(writer);
	const mdx::TrackHeader<float> read(reader, 0);
	CHECK(reader.remaining() == 0);
	REQUIRE(read.tracks.size() == 3);
	CHECK(read.tracks[2].frame == 20);
	CHECK(read.tracks[2].value == 2.f);

	// quat: 4 + 16 = 20 bytes per track, the largest value type.
	BinaryWriter quat_writer;
	quat_writer.write<uint32_t>(2);
	quat_writer.write<uint32_t>(1);
	quat_writer.write<int32_t>(-1);
	for (int i = 0; i < 2; i++) {
		quat_writer.write<int32_t>(i * 10);
		quat_writer.write<float>(0.f);
		quat_writer.write<float>(0.f);
		quat_writer.write<float>(0.f);
		quat_writer.write<float>(1.f);
	}

	BinaryReader quat_reader = reader_from(quat_writer);
	const mdx::TrackHeader<glm::quat> quat_read(quat_reader, 0);
	CHECK(quat_reader.remaining() == 0);
	REQUIRE(quat_read.tracks.size() == 2);
	CHECK(quat_read.tracks[1].frame == 10);
}
