#define DOCTEST_CONFIG_USE_STD_HEADERS
#include <doctest/doctest.h>

import std;
import types;
import SLK;
import INI;
import Hierarchy;
import PathingTexture;
import Texture;
import ResourceManager;
import BinaryReader;
import no_init_allocator;

namespace fs = std::filesystem;

// Asana 1220364334409503: the stock 3.0.1 map (1)ForgottenHollow logged model and texture load errors.

namespace {
	/// Points hierarchy.map_directory at a fresh scratch folder and restores it, and the aliases, on every exit.
	struct ScratchMapDirectory {
		fs::path previous_directory = hierarchy.map_directory;
		decltype(hierarchy.aliases.json_data) previous_aliases = hierarchy.aliases.json_data;
		bool previous_hd = hierarchy.hd;
		fs::path dir;

		explicit ScratchMapDirectory(const std::string& name) {
			hierarchy.hd = false; // Texture adds _diffuse in HD
			dir = fs::temp_directory_path() / "hivewe_missing_asset_fallback_test" / name;
			std::error_code ec;
			fs::remove_all(dir, ec);
			fs::create_directories(dir, ec);
			hierarchy.map_directory = dir;
		}
		~ScratchMapDirectory() {
			hierarchy.map_directory = previous_directory;
			hierarchy.aliases.json_data = previous_aliases;
			hierarchy.hd = previous_hd;
			std::error_code ec;
			fs::remove_all(dir, ec);
		}
		ScratchMapDirectory(const ScratchMapDirectory&) = delete;
		ScratchMapDirectory& operator=(const ScratchMapDirectory&) = delete;
	};

	void write_bytes(const fs::path& path, const std::vector<u8>& bytes) {
		std::error_code ec;
		fs::create_directories(path.parent_path(), ec);
		std::ofstream out(path, std::ios::binary);
		out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}

	void put_u32(std::vector<u8>& bytes, const u32 value) {
		for (int i = 0; i < 4; i++) {
			bytes.push_back(static_cast<u8>(value >> (8 * i)));
		}
	}

	/// A 4x4 DXT5 DDS whose one block is opaque pure red.
	std::vector<u8> make_dxt5_dds() {
		std::vector<u8> bytes = { 'D', 'D', 'S', ' ' };
		put_u32(bytes, 124);											// dwSize
		put_u32(bytes, 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000 | 0x80000); // caps, height, width, pixelformat, mipmapcount, linearsize
		put_u32(bytes, 4);												// height
		put_u32(bytes, 4);												// width
		put_u32(bytes, 16);											// linear size
		put_u32(bytes, 0);												// depth
		put_u32(bytes, 1);												// mipmap count
		for (int i = 0; i < 11; i++) {
			put_u32(bytes, 0); // reserved
		}
		put_u32(bytes, 32);	 // pixel format size
		put_u32(bytes, 0x4); // DDPF_FOURCC
		bytes.insert(bytes.end(), { 'D', 'X', 'T', '5' });
		for (int i = 0; i < 5; i++) {
			put_u32(bytes, 0); // bit count and masks
		}
		put_u32(bytes, 0x1000); // DDSCAPS_TEXTURE
		for (int i = 0; i < 4; i++) {
			put_u32(bytes, 0); // caps2, caps3, caps4, reserved2
		}
		// DXT5 block: alpha 255/255 with index 0 everywhere, then color0 = color1 = 0xF800 (red) with index 0 everywhere.
		bytes.insert(bytes.end(), { 255, 255, 0, 0, 0, 0, 0, 0, 0x00, 0xF8, 0x00, 0xF8, 0, 0, 0, 0 });
		return bytes;
	}

	/// A 2x2 uncompressed 24-bit TGA.
	std::vector<u8> make_tga() {
		std::vector<u8> bytes = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 24, 0 };
		for (int i = 0; i < 4; i++) {
			bytes.insert(bytes.end(), { 0, 255, 0 });
		}
		return bytes;
	}

	/// A 2x2 paletted BLP1 without alpha whose pixels all use palette entry 0, pure red (stored BGRA).
	std::vector<u8> make_blp1() {
		std::vector<u8> bytes = { 'B', 'L', 'P', '1' };
		put_u32(bytes, 1); // paletted
		put_u32(bytes, 0); // alpha bits
		put_u32(bytes, 2); // width
		put_u32(bytes, 2); // height
		put_u32(bytes, 0); // extra
		put_u32(bytes, 0); // has mipmaps
		constexpr u32 pixels_offset = 28 + 16 * 4 + 16 * 4 + 256 * 4;
		put_u32(bytes, pixels_offset);
		for (int i = 1; i < 16; i++) {
			put_u32(bytes, 0);
		}
		put_u32(bytes, 4);
		for (int i = 1; i < 16; i++) {
			put_u32(bytes, 0);
		}
		put_u32(bytes, 0x00FF0000); // palette 0: B=0, G=0, R=255
		for (int i = 1; i < 256; i++) {
			put_u32(bytes, 0);
		}
		bytes.insert(bytes.end(), { 0, 0, 0, 0 });
		return bytes;
	}

	slk::SLK make_doodad_meta() {
		slk::SLK meta;
		meta.add_row("dfil");
		meta.set_shadow_data("field", "dfil", "file");
		meta.set_shadow_data("type", "dfil", "model");
		meta.set_shadow_data("data", "dfil", "0");
		meta.build_meta_map();
		return meta;
	}

	slk::SLK make_doodads(const std::vector<std::string>& ids) {
		slk::SLK data;
		for (const auto& id : ids) {
			data.add_row(id);
			data.base_data[id] = {};
		}
		return data;
	}
} // namespace

TEST_CASE("INI merge matches a section that differs from the row id only in letter case when asked to") {
	const slk::SLK meta = make_doodad_meta();
	ini::INI skins;
	skins.ini_data["YCgd"]["file"] = { "Doodads/X" };

	slk::SLK exact_only = make_doodads({ "Ycgd" });
	exact_only.merge(skins, meta);
	CHECK(exact_only.data("file", "Ycgd").empty());

	slk::SLK doodads = make_doodads({ "Ycgd" });
	doodads.merge(skins, meta, true);
	CHECK(doodads.data("file", "Ycgd") == "Doodads/X");
	CHECK_FALSE(doodads.base_data.contains("YCgd"));
}

TEST_CASE("INI merge keeps the exact section when a case-only section also names the row") {
	const slk::SLK meta = make_doodad_meta();
	ini::INI skins;
	// The exact section is inserted first, so the case-only one comes later in iteration and would overwrite it.
	skins.ini_data["Ycgd"]["file"] = { "Doodads/Exact" };
	skins.ini_data["YCGD"]["file"] = { "Doodads/Other" };

	slk::SLK doodads = make_doodads({ "Ycgd" });
	doodads.merge(skins, meta, true);
	CHECK(doodads.data("file", "Ycgd") == "Doodads/Exact");
}

TEST_CASE("INI merge merges nothing when a section matches several rows only by letter case") {
	const slk::SLK meta = make_doodad_meta();
	ini::INI skins;
	skins.ini_data["abcd"]["file"] = { "Doodads/X" };

	slk::SLK doodads = make_doodads({ "Abcd", "ABCD" });
	doodads.merge(skins, meta, true);
	CHECK(doodads.data("file", "Abcd").empty());
	CHECK(doodads.data("file", "ABCD").empty());
}

TEST_CASE("alias_candidates tries the alias target, then its .dds twin") {
	CHECK(alias_candidates("Textures\\Cliff0Legacy.blp") == std::vector<std::string> { "Textures\\Cliff0Legacy.blp", "Textures\\Cliff0Legacy.dds" });
	CHECK(alias_candidates("Textures\\Ghost2.TGA") == std::vector<std::string> { "Textures\\Ghost2.TGA", "Textures\\Ghost2.dds" });
	CHECK(alias_candidates("Textures\\Upper.BLP") == std::vector<std::string> { "Textures\\Upper.BLP", "Textures\\Upper.dds" });
	CHECK(alias_candidates("Textures\\Already.DDS") == std::vector<std::string> { "Textures\\Already.DDS" });
	// Non-texture targets never get the .dds retry, so they cannot receive texture bytes.
	CHECK(alias_candidates("Textures\\NoExtension") == std::vector<std::string> { "Textures\\NoExtension" });
	CHECK(alias_candidates("Units\\Footman.mdx") == std::vector<std::string> { "Units\\Footman.mdx" });
	CHECK(alias_candidates("Sound\\Hit.wav") == std::vector<std::string> { "Sound\\Hit.wav" });
	CHECK(alias_candidates("Units\\UnitData.slk") == std::vector<std::string> { "Units\\UnitData.slk" });
	CHECK(alias_candidates("Units\\UnitSkin.txt") == std::vector<std::string> { "Units\\UnitSkin.txt" });
}

TEST_CASE("an alias to a model whose .dds twin exists does not open the .dds") {
	ScratchMapDirectory scratch("alias_model");
	write_bytes(scratch.dir / "HiveWEAliasTest" / "Model.dds", make_dxt5_dds());
	hierarchy.aliases.json_data["hivewealiastest\\old.mdx"] = "HiveWEAliasTest\\Model.mdx";
	CHECK_FALSE(hierarchy.open_file("HiveWEAliasTest/Old.mdx").has_value());
	CHECK_FALSE(hierarchy.file_exists("HiveWEAliasTest/Old.mdx"));
}

TEST_CASE("an alias whose target exists only as .dds opens") {
	ScratchMapDirectory scratch("alias_dds");
	write_bytes(scratch.dir / "HiveWEAliasTest" / "Cliff0Legacy.dds", make_dxt5_dds());
	hierarchy.aliases.json_data["hivewealiastest\\cliff0.blp"] = "HiveWEAliasTest\\Cliff0Legacy.blp";

	const auto opened = hierarchy.open_file("HiveWEAliasTest/Cliff0.blp");
	REQUIRE(opened.has_value());
	CHECK(opened->buffer.size() == make_dxt5_dds().size());
	CHECK(hierarchy.file_exists("HiveWEAliasTest/Cliff0.blp"));

	const auto missing = hierarchy.open_file("HiveWEAliasTest/Missing.blp");
	CHECK_FALSE(missing.has_value());
}

TEST_CASE("a pathing texture named .tga loads from the .dds next to it") {
	ScratchMapDirectory scratch("pathing_dds");
	write_bytes(scratch.dir / "HiveWEPathingTest" / "Unpathable4x4.dds", make_dxt5_dds());

	const PathingTexture texture("HiveWEPathingTest/Unpathable4x4.tga");
	CHECK(texture.width == 4);
	CHECK(texture.height == 4);
	REQUIRE(texture.channels > 0);
	CHECK(texture.data.size() == static_cast<size_t>(16 * texture.channels));
	CHECK(texture.data[0] == 255); // red
	CHECK(texture.homogeneous);
}

TEST_CASE("a pathing texture prefers the exact file over another extension") {
	ScratchMapDirectory scratch("pathing_exact");
	write_bytes(scratch.dir / "HiveWEPathingTest" / "Both.dds", make_dxt5_dds());
	write_bytes(scratch.dir / "HiveWEPathingTest" / "Both.tga", make_tga());

	const PathingTexture texture("HiveWEPathingTest/Both.tga");
	CHECK(texture.width == 2);
	CHECK(texture.height == 2);
}

// The .blp probe of a texture loader now gets .dds bytes through the alias; decoding them as BLP crashed the stock map resave.
TEST_CASE("a texture whose .blp alias resolves to a .dds decodes the .dds") {
	ScratchMapDirectory scratch("texture_alias_dds");
	write_bytes(scratch.dir / "HiveWEAliasTest" / "Cliff0Legacy.dds", make_dxt5_dds());
	hierarchy.aliases.json_data["hivewealiastest\\cliff0.blp"] = "HiveWEAliasTest\\Cliff0Legacy.blp";

	const Texture texture("HiveWEAliasTest/Cliff0");
	CHECK(texture.width == 4);
	CHECK(texture.height == 4);
	REQUIRE(texture.channels > 0);
	CHECK(texture.data.size() == static_cast<size_t>(16 * texture.channels));
	CHECK(texture.data[0] == 255); // red
}

TEST_CASE("a pathing texture named .tga that holds BLP1 bytes decodes as BLP") {
	ScratchMapDirectory scratch("pathing_blp_named_tga");
	write_bytes(scratch.dir / "HiveWEPathingTest" / "Blp.tga", make_blp1());

	const PathingTexture texture("HiveWEPathingTest/Blp.tga");
	CHECK(texture.width == 2);
	CHECK(texture.height == 2);
	CHECK(texture.channels == 4);
	CHECK(texture.data[0] == 255); // red after the BGRA swap
}

namespace {
	/// Without CASC (as under ctest) the placeholder is the generated 64x64 magenta image.
	void check_generated_placeholder(const Texture& texture) {
		CHECK(texture.width == 64);
		CHECK(texture.height == 64);
		REQUIRE(texture.channels == 4);
		REQUIRE(texture.data.size() == 64 * 64 * 4);
		CHECK(texture.data[0] == 255);
		CHECK(texture.data[1] == 0);
		CHECK(texture.data[2] == 255);
		CHECK(texture.data[3] == 255);
	}
} // namespace

// SOIL returns nullptr for these. Texture callers take .value(), so the file gets a placeholder instead of an error (a garbage
// Lords_Blight.blp used to stop --resave of data/test map).
TEST_CASE("an undecodable texture loads as a placeholder through ResourceManager") {
	ScratchMapDirectory scratch("texture_undecodable");
	write_bytes(scratch.dir / "HiveWEBadTexture" / "Empty.blp", {});
	write_bytes(scratch.dir / "HiveWEBadTexture" / "Garbage.tga", { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 });
	std::vector<u8> cut_blp = { 'B', 'L', 'P', '1', 1, 0, 0, 0 }; // BLP1 magic, header cut short
	write_bytes(scratch.dir / "HiveWEBadTexture" / "Cut.blp", cut_blp);
	std::vector<u8> bad_dds = make_dxt5_dds();
	bad_dds.resize(40); // header cut short
	write_bytes(scratch.dir / "HiveWEBadTexture" / "Broken.dds", bad_dds);
	hierarchy.aliases.json_data["hivewebadtexture\\aliased.blp"] = "HiveWEBadTexture\\Broken.blp";

	for (const char* name : { "HiveWEBadTexture/Empty", "HiveWEBadTexture/Garbage", "HiveWEBadTexture/Cut", "HiveWEBadTexture/Aliased" }) {
		CAPTURE(name);
		const auto texture = resource_manager.load<Texture>(name);
		REQUIRE(texture.has_value());
		check_generated_placeholder(**texture);
	}
}

TEST_CASE("decode_image refuses undecodable bytes without throwing and the placeholder is a valid ground tile atlas") {
	std::vector<u8> cut_blp = { 'B', 'L', 'P', '1', 1, 0, 0, 0 };
	BinaryReader reader(std::vector<u8, default_init_allocator<u8>>(cut_blp.begin(), cut_blp.end()));
	int width = -1;
	int height = -1;
	int channels = -1;
	std::vector<u8> pixels { 1 };
	CHECK_FALSE(decode_image(reader, width, height, channels, pixels));
	CHECK(pixels.empty());

	ScratchMapDirectory scratch("placeholder");
	placeholder_image(width, height, channels, pixels);
	// GroundTexture reads 4x4 tiles of height / 4 pixels from rows of `width` pixels with 3 or 4 channels.
	CHECK(width >= height);
	CHECK(height % 4 == 0);
	CHECK((channels == 3 || channels == 4));
	CHECK(pixels.size() == static_cast<size_t>(width) * height * channels);
}

TEST_CASE("an undecodable pathing texture still gives a ResourceManager error") {
	ScratchMapDirectory scratch("pathing_undecodable");
	write_bytes(scratch.dir / "HiveWEBadTexture" / "EmptyPathing.blp", {});

	const auto pathing = resource_manager.load<PathingTexture>("HiveWEBadTexture/EmptyPathing.blp");
	REQUIRE_FALSE(pathing.has_value());
	CHECK(pathing.error().find("EmptyPathing") != std::string::npos);
}

TEST_CASE("a missing pathing texture throws an error naming the file") {
	ScratchMapDirectory scratch("pathing_missing");
	try {
		const PathingTexture texture("HiveWEPathingTest/Nowhere.tga");
		FAIL("expected a throw");
	} catch (const std::runtime_error& e) {
		CHECK(std::string(e.what()).find("Nowhere.tga") != std::string::npos);
	}
}
