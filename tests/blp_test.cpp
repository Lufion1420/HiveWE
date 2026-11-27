#define DOCTEST_CONFIG_USE_STD_HEADERS
#include <doctest/doctest.h>

import std;
import types;
import BLP;
import BinaryReader;
import no_init_allocator;
import Utilities;

namespace fs = std::filesystem;

// Asana 1220963622495886: a model's folder can hand the BLP1 decoder any file, so a damaged one must fail cleanly.

namespace {
	struct Decoded {
		bool ok = false;
		int width = 0;
		int height = 0;
		std::vector<u8> pixels;
	};

	Decoded decode(const std::vector<u8>& bytes) {
		BinaryReader reader(std::vector<u8, default_init_allocator<u8>>(bytes.begin(), bytes.end()));
		Decoded result;
		int channels = 0;
		u8* data = blp::load(reader, result.width, result.height, channels);
		if (data != nullptr) {
			result.ok = true;
			result.pixels.assign(data, data + static_cast<std::size_t>(result.width) * result.height * channels);
			delete[] data;
		}
		return result;
	}

	void set_u32(std::vector<u8>& bytes, const std::size_t offset, const u32 value) {
		for (int i = 0; i < 4; i++) {
			bytes[offset + i] = static_cast<u8>(value >> (8 * i));
		}
	}

	u32 get_u32(const std::vector<u8>& bytes, const std::size_t offset) {
		u32 value = 0;
		for (int i = 0; i < 4; i++) {
			value |= static_cast<u32>(bytes[offset + i]) << (8 * i);
		}
		return value;
	}

	// Header: magic, content type, alpha bits, width, height, two more dwords, 16 mipmap offsets, 16 mipmap sizes.
	constexpr std::size_t width_at = 12;
	constexpr std::size_t height_at = 16;
	constexpr std::size_t offset_at = 28;
	constexpr std::size_t size_at = 92;
	constexpr std::size_t body_at = 156;

	/// A 2x2 paletted BLP1 with 8 bit alpha. Palette entry n is (B, G, R) = (n, 2, 3).
	std::vector<u8> make_paletted() {
		std::vector<u8> bytes(body_at + 1024 + 8, 0);
		std::ranges::copy(std::string_view("BLP1"), bytes.begin());
		set_u32(bytes, 4, 1);
		set_u32(bytes, 8, 8);
		set_u32(bytes, width_at, 2);
		set_u32(bytes, height_at, 2);
		set_u32(bytes, offset_at, body_at + 1024);
		set_u32(bytes, size_at, 8);
		for (u32 n = 0; n < 256; n++) {
			set_u32(bytes, body_at + n * 4, n | (2 << 8) | (3 << 16));
		}
		const std::array<u8, 8> pixels = { 10, 20, 30, 40, 255, 128, 64, 0 }; // indices, then alpha
		std::ranges::copy(pixels, bytes.begin() + body_at + 1024);
		return bytes;
	}

	/// The JPEG BLP1 (256x256) of the shipped test map.
	std::vector<u8> jpeg_blp() {
		auto reader = read_file(fs::path(TEST_MAP_DIR) / "war3mapMap.blp");
		REQUIRE(reader.has_value());
		return std::vector<u8>(reader->buffer.begin(), reader->buffer.end());
	}
}

TEST_CASE("blp: a paletted image decodes to its palette colours as RGBA") {
	const Decoded image = decode(make_paletted());
	REQUIRE(image.ok);
	CHECK(image.width == 2);
	CHECK(image.height == 2);
	CHECK(image.pixels == std::vector<u8> { 3, 2, 10, 255, 3, 2, 20, 128, 3, 2, 30, 64, 3, 2, 40, 0 });
}

TEST_CASE("blp: a file cut short fails without throwing") {
	const std::vector<u8> jpeg = jpeg_blp();
	const std::vector<u8> paletted = make_paletted();
	for (const std::size_t length : std::vector<std::size_t> { 0, 4, 8, 20, 100, 158, 700, jpeg.size() / 2, jpeg.size() - 200 }) {
		CAPTURE(length);
		Decoded image;
		CHECK_NOTHROW(image = decode(std::vector<u8>(jpeg.begin(), jpeg.begin() + length)));
		CHECK_FALSE(image.ok);
	}
	for (const std::size_t length : std::vector<std::size_t> { 100, 160, body_at + 1024, paletted.size() - 1 }) {
		CAPTURE(length);
		Decoded image;
		CHECK_NOTHROW(image = decode(std::vector<u8>(paletted.begin(), paletted.begin() + length)));
		CHECK_FALSE(image.ok);
	}
}

TEST_CASE("blp: a complete JPEG whose declared size runs past the file still decodes") {
	std::vector<u8> bytes = jpeg_blp();
	const Decoded original = decode(bytes);
	REQUIRE(original.ok);
	CHECK(original.width == 256);
	CHECK(original.height == 256);

	set_u32(bytes, size_at, get_u32(bytes, size_at) + 5000);
	const Decoded overlong = decode(bytes);
	REQUIRE(overlong.ok);
	CHECK(overlong.pixels == original.pixels);
}

TEST_CASE("blp: a JPEG mipmap offset outside the file fails") {
	for (const u32 offset : std::vector<u32> { 0x07000000u, 0xFFFFFFFFu }) {
		CAPTURE(offset);
		std::vector<u8> bytes = jpeg_blp();
		set_u32(bytes, offset_at, offset);
		CHECK_FALSE(decode(bytes).ok);
	}
}

TEST_CASE("blp: a JPEG header size outside the file fails") {
	std::vector<u8> bytes = jpeg_blp();
	set_u32(bytes, body_at, 0xFFFFFF00u);
	CHECK_FALSE(decode(bytes).ok);
}

TEST_CASE("blp: a paletted mipmap offset outside the file fails") {
	for (const u32 offset : std::vector<u32> { 5000u, 0x07000000u, 0xFFFFFFFFu }) {
		CAPTURE(offset);
		std::vector<u8> bytes = make_paletted();
		set_u32(bytes, offset_at, offset);
		CHECK_FALSE(decode(bytes).ok);
	}
}

TEST_CASE("blp: impossible dimensions fail") {
	for (const u32 side : std::vector<u32> { 0u, 16385u, 262144u, 0x80000000u }) {
		CAPTURE(side);
		std::vector<u8> paletted = make_paletted();
		set_u32(paletted, width_at, side);
		set_u32(paletted, height_at, side);
		CHECK_FALSE(decode(paletted).ok);

		std::vector<u8> jpeg = jpeg_blp();
		set_u32(jpeg, width_at, side);
		set_u32(jpeg, height_at, side);
		CHECK_FALSE(decode(jpeg).ok);
	}
}

TEST_CASE("blp: a JPEG that cannot be scaled into the header's image fails") {
	// Same width, far fewer rows: no scale of the JPEG fits.
	std::vector<u8> bytes = jpeg_blp();
	set_u32(bytes, height_at, 16);
	CHECK_FALSE(decode(bytes).ok);

	// Narrower and much flatter.
	bytes = jpeg_blp();
	set_u32(bytes, width_at, 128);
	set_u32(bytes, height_at, 4);
	CHECK_FALSE(decode(bytes).ok);

	// Half size both ways: the decoder scales the JPEG down, as before.
	bytes = jpeg_blp();
	set_u32(bytes, width_at, 128);
	set_u32(bytes, height_at, 128);
	CHECK(decode(bytes).ok);
}
