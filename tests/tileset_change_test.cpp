// doctest forward-declares std::char_traits<char>; std::string needs the real one before doctest.h when only `import std` follows.
#include <string>
#include <doctest/doctest.h>

import std;
import TilesetChange;

namespace fs = std::filesystem;

// Terrain::change_tileset needs a GL context, so these cover what it and the Change Tileset dialog decide: which slot each old tile
// moves to, how the corner texture indices are rewritten (blight included), the custom tile policy, and the file writing of the OK
// path (Asana 1220841004647015).

namespace {
	const std::vector<std::string> original = { "Adrt", "Agrs", "Arck", "Agrd", "Avin" };
}

TEST_CASE("build_tile_remap keeps kept tiles at their new slot and sends removed tiles to their replacement") {
	// Agrd and Avin are removed, Lgrs is new and put first, Agrs moves.
	const std::vector<std::string> new_ids = { "Lgrs", "Adrt", "Arck", "Agrs" };
	const std::map<std::string, std::string> replacements = { { "Agrd", "Agrs" }, { "Avin", "Lgrs" } };

	const std::vector<int> remap = build_tile_remap(original, new_ids, replacements);

	REQUIRE(remap.size() == original.size());
	CHECK(remap[0] == 1); // Adrt
	CHECK(remap[1] == 3); // Agrs
	CHECK(remap[2] == 2); // Arck
	CHECK(remap[3] == 3); // Agrd -> Agrs
	CHECK(remap[4] == 0); // Avin -> Lgrs
}

TEST_CASE("build_tile_remap falls back to slot 0 for a missing or stale replacement") {
	const std::vector<std::string> new_ids = { "Adrt", "Agrs" };
	// Arck is the replacement of Agrd but is not in the new tileset itself; Avin has no entry at all.
	const std::map<std::string, std::string> replacements = { { "Agrd", "Arck" } };

	const std::vector<int> remap = build_tile_remap(original, new_ids, replacements);

	REQUIRE(remap.size() == original.size());
	CHECK(remap[0] == 0); // Adrt kept at slot 0
	CHECK(remap[1] == 1); // Agrs kept at slot 1
	CHECK(remap[2] == 0); // Arck removed, no replacement
	CHECK(remap[3] == 0); // Agrd -> stale Arck
	CHECK(remap[4] == 0); // Avin removed, no entry
}

TEST_CASE("build_tile_remap of an empty new tileset is empty") {
	CHECK(build_tile_remap(original, {}, {}).empty());
}

TEST_CASE("remap_ground_textures rewrites tile indices and moves the blight index") {
	const std::vector<std::string> new_ids = { "Lgrs", "Adrt", "Arck", "Agrs" };
	const std::vector<int> remap = build_tile_remap(original, new_ids, { { "Agrd", "Agrs" }, { "Avin", "Lgrs" } });

	// One corner per old tile, then blight (index 5, one past the old tiles).
	std::vector<std::uint8_t> corners = { 0, 1, 2, 3, 4, 5 };
	remap_ground_textures(corners, remap, new_ids.size());

	CHECK(corners == std::vector<std::uint8_t> { 1, 3, 2, 3, 0, 4 });
}

TEST_CASE("remap_ground_textures sends an out-of-range index to slot 0 instead of reading past the table") {
	std::vector<std::uint8_t> corners = { 2, 6, 63 };
	remap_ground_textures(corners, std::vector<int> { 0, 1 }, 2);

	CHECK(corners[0] == 2); // one past the old tiles is blight, which moves behind the new tiles
	CHECK(corners[1] == 0);
	CHECK(corners[2] == 0);
}

TEST_CASE("remap_ground_textures with an identical tileset changes nothing") {
	const std::vector<int> identity = build_tile_remap(original, original, {});
	std::vector<std::uint8_t> corners = { 0, 1, 2, 3, 4, 5, 2, 2 };
	const auto before = corners;

	remap_ground_textures(corners, identity, original.size());

	CHECK(corners == before);
}

// Undo snapshots and the terrain clipboard hold ground texture indices: they must be dropped when the tile list changes.
TEST_CASE("tile_indices_invalidated is true when a tile is removed, added or moved") {
	CHECK(tile_indices_invalidated(original, std::vector<std::string> { "Adrt", "Agrs", "Arck", "Agrd" })); // last removed
	CHECK(tile_indices_invalidated(original, std::vector<std::string> { "Adrt", "Arck", "Agrd", "Avin" })); // one removed in the middle
	CHECK(tile_indices_invalidated(original, std::vector<std::string> { "Adrt" }));                         // two or more removed
	CHECK(tile_indices_invalidated(original, std::vector<std::string> { "Adrt", "Agrs", "Arck", "Agrd", "Avin", "Lgrs" })); // added
	CHECK(tile_indices_invalidated(original, std::vector<std::string> { "Agrs", "Adrt", "Arck", "Agrd", "Avin" }));         // reordered
}

TEST_CASE("tile_indices_invalidated is false for the same list, so a texture-only change keeps undo and clipboard") {
	CHECK_FALSE(tile_indices_invalidated(original, original));
	CHECK_FALSE(tile_indices_invalidated({}, {}));
}

// The unresolved Warcraft III behaviour (NEEDS_GAME_TEST) is pinned here: change tileset_policy() and these checks together.

TEST_CASE("tileset_policy pins the unverified Warcraft III assumptions") {
	const TilesetPolicy sd = tileset_policy(false);
	CHECK(sd.max_tiles == 64);
	CHECK(sd.write_sd_override);
	CHECK_FALSE(sd.name_hd_override);
	CHECK(tileset_policy(true).name_hd_override);
}

TEST_CASE("tile_override_names builds the SD and HD override names") {
	const std::vector<std::string> sd = tile_override_names(tileset_policy(false), "TerrainArt\\LordaeronSummer", "Lords_Grass", "blp");
	REQUIRE(sd.size() == 1);
	CHECK(sd[0] == "TerrainArt/LordaeronSummer/Lords_Grass.blp");

	const std::vector<std::string> hd = tile_override_names(tileset_policy(true), "TerrainArt/LordaeronSummer", "Lords_Grass", "tga");
	REQUIRE(hd.size() == 2);
	CHECK(hd[0] == "TerrainArt/LordaeronSummer/Lords_Grass.tga");
	CHECK(hd[1] == "_hd.w3mod:TerrainArt/LordaeronSummer/Lords_Grass_diffuse.tga");
}

TEST_CASE("only BLP and TGA override a tile in SD, because a map DDS never beats the stock BLP") {
	CHECK(is_tile_texture_extension("blp"));
	CHECK(is_tile_texture_extension("tga"));
	CHECK_FALSE(is_tile_texture_extension("dds"));
	CHECK_FALSE(is_tile_texture_extension("png"));

	CHECK(tile_override_names(tileset_policy(false), "TerrainArt/X", "Y", "dds").empty());
	CHECK(tile_override_names(tileset_policy(false), "TerrainArt/X", "Y", "png").empty());
	CHECK(tile_override_names(tileset_policy(false), "TerrainArt/X", "Y", "BLP").empty()); // callers pass lower case

	TilesetPolicy off = tileset_policy(false);
	off.write_sd_override = false;
	CHECK(tile_override_names(off, "TerrainArt/X", "Y", "blp").empty());
}

TEST_CASE("is_writable_map_file_name accepts a relative path inside the map") {
	CHECK(is_writable_map_file_name("TerrainArt/LordaeronSummer/Lords_Grass.blp"));
	CHECK(is_writable_map_file_name("TerrainArt\\LordaeronSummer\\Lords_Grass.blp"));
	CHECK(is_writable_map_file_name("a/./b.blp"));
}

TEST_CASE("is_writable_map_file_name refuses names that leave the map folder") {
	CHECK_FALSE(is_writable_map_file_name(""));
	CHECK_FALSE(is_writable_map_file_name("_hd.w3mod:TerrainArt/LordaeronSummer/Lords_Grass_diffuse.blp")); // colon
	CHECK_FALSE(is_writable_map_file_name("C:/Windows/x.blp"));         // drive
	CHECK_FALSE(is_writable_map_file_name("C:x.blp"));                  // drive-relative
	CHECK_FALSE(is_writable_map_file_name("/etc/x.blp"));               // leading slash
	CHECK_FALSE(is_writable_map_file_name("\\Windows\\x.blp"));         // leading backslash
	CHECK_FALSE(is_writable_map_file_name("\\\\server\\share\\x.blp")); // UNC
	CHECK_FALSE(is_writable_map_file_name("../x.blp"));
	CHECK_FALSE(is_writable_map_file_name("a/../../x.blp"));
	CHECK_FALSE(is_writable_map_file_name("a\\..\\x.blp"));
	CHECK_FALSE(is_writable_map_file_name("a/b/.."));
	CHECK_FALSE(is_writable_map_file_name("a/b*.blp")); // wildcard
	CHECK_FALSE(is_writable_map_file_name("a/b.blp ")); // trailing space
}

TEST_CASE("is_archive_only_name recognises the HD archive form") {
	CHECK(is_archive_only_name("_hd.w3mod:TerrainArt/X/Y_diffuse.blp"));
	CHECK_FALSE(is_archive_only_name("TerrainArt/X/Y.blp"));
}

namespace {
	fs::path make_scratch_dir(const std::string& name) {
		const fs::path dir = fs::temp_directory_path() / "hivewe_tileset_change_test" / name;
		std::error_code ec;
		fs::remove_all(dir, ec);
		fs::create_directories(dir, ec);
		return dir;
	}

	void write_all(const fs::path& path, const std::string& content) {
		fs::create_directories(path.parent_path());
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		out << content;
	}

	std::string read_all(const fs::path& path) {
		std::ifstream in(path, std::ios::binary);
		return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
	}

	std::size_t file_count(const fs::path& dir) {
		std::size_t count = 0;
		std::error_code ec;
		for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
			count += entry.is_regular_file() ? 1 : 0;
		}
		return count;
	}

	TileOverride grass(const fs::path& source, const std::string& dir = "TerrainArt\\LordaeronSummer") {
		return { "Lgrs", dir, "Lords_Grass", "blp", source };
	}
}

// The OK path of the dialog: write_tile_overrides is what TileSetter::save_tiles calls before Terrain::change_tileset. Cancel never
// calls it, so "nothing imported" must write nothing.

TEST_CASE("write_tile_overrides without any import writes nothing") {
	const fs::path map = make_scratch_dir("no_import");

	const auto result = write_tile_overrides(tileset_policy(false), map, {});

	REQUIRE(result.has_value());
	CHECK(result->empty());
	CHECK(file_count(map) == 0);
}

TEST_CASE("write_tile_overrides puts the file at the stock tile path and leaves no temporary file") {
	const fs::path root = make_scratch_dir("write_ok");
	const fs::path map = root / "map";
	write_all(root / "marble.blp", "MARBLE");
	const std::vector<TileOverride> overrides = { grass(root / "marble.blp") };

	const auto result = write_tile_overrides(tileset_policy(false), map, overrides);

	REQUIRE_MESSAGE(result.has_value(), result.error().message);
	REQUIRE(result->size() == 1);
	CHECK(read_all(map / "TerrainArt/LordaeronSummer/Lords_Grass.blp") == "MARBLE");
	CHECK(file_count(map) == 1);
}

TEST_CASE("write_tile_overrides replaces an existing override") {
	const fs::path root = make_scratch_dir("replace");
	const fs::path map = root / "map";
	write_all(root / "new.blp", "NEW");
	write_all(map / "TerrainArt/LordaeronSummer/Lords_Grass.blp", "OLD");

	const auto result = write_tile_overrides(tileset_policy(false), map, std::vector<TileOverride> { grass(root / "new.blp") });

	REQUIRE(result.has_value());
	CHECK(read_all(map / "TerrainArt/LordaeronSummer/Lords_Grass.blp") == "NEW");
	CHECK(file_count(map) == 1);
}

TEST_CASE("write_tile_overrides copies nothing for the HD archive name") {
	const fs::path root = make_scratch_dir("hd_skipped");
	const fs::path map = root / "map";
	write_all(root / "marble.blp", "MARBLE");

	const auto result = write_tile_overrides(tileset_policy(true), map, std::vector<TileOverride> { grass(root / "marble.blp") });

	REQUIRE(result.has_value());
	CHECK(result->size() == 1); // the SD file only
	CHECK(file_count(map) == 1);
}

TEST_CASE("write_tile_overrides is all or nothing when a later copy fails") {
	const fs::path root = make_scratch_dir("second_fails");
	const fs::path map = root / "map";
	write_all(root / "good.blp", "GOOD");
	write_all(map / "TerrainArt/LordaeronSummer/Lords_Grass.blp", "ORIGINAL");
	write_all(map / "keep.txt", "KEEP");
	const std::vector<TileOverride> overrides = {
		grass(root / "good.blp"),
		{ "Ldrt", "TerrainArt\\Other", "Lords_Dirt", "blp", root / "missing.blp" }, // does not exist: the copy fails
	};

	const auto result = write_tile_overrides(tileset_policy(false), map, overrides);

	REQUIRE_FALSE(result.has_value());
	CHECK(result.error().message.contains("missing.blp"));
	// The first tile's existing file is untouched, no temporary or set-aside file remains, the new folder is gone.
	CHECK(read_all(map / "TerrainArt/LordaeronSummer/Lords_Grass.blp") == "ORIGINAL");
	CHECK(read_all(map / "keep.txt") == "KEEP");
	CHECK(file_count(map) == 2);
	CHECK_FALSE(fs::exists(map / "TerrainArt/Other"));
}

TEST_CASE("write_tile_overrides is all or nothing when a rename fails") {
	const fs::path root = make_scratch_dir("rename_fails");
	const fs::path map = root / "map";
	write_all(root / "good.blp", "GOOD");
	write_all(map / "TerrainArt/LordaeronSummer/Lords_Grass.blp", "ORIGINAL");
	// The second tile already has a file, and its set-aside name is taken by a non-empty folder: the copy to the temporary name works,
	// moving the old file aside does not.
	write_all(map / "TerrainArt/Other/Lords_Dirt.blp", "DIRT");
	write_all(map / "TerrainArt/Other/Lords_Dirt.blp.old.hivewe-tmp/blocker.txt", "BLOCK");
	const std::vector<TileOverride> overrides = {
		grass(root / "good.blp"),
		{ "Ldrt", "TerrainArt\\Other", "Lords_Dirt", "blp", root / "good.blp" },
	};

	const auto result = write_tile_overrides(tileset_policy(false), map, overrides);

	REQUIRE_FALSE(result.has_value());
	CHECK(read_all(map / "TerrainArt/LordaeronSummer/Lords_Grass.blp") == "ORIGINAL");
	CHECK(read_all(map / "TerrainArt/Other/Lords_Dirt.blp") == "DIRT");
	CHECK(read_all(map / "TerrainArt/Other/Lords_Dirt.blp.old.hivewe-tmp/blocker.txt") == "BLOCK");
	CHECK(file_count(map) == 3); // grass, dirt and the blocker: no temporary file left
}

TEST_CASE("write_tile_overrides refuses a tile dir that leaves the map folder and writes nothing") {
	const fs::path root = make_scratch_dir("escape");
	const fs::path map = root / "map";
	fs::create_directories(map);
	write_all(root / "good.blp", "GOOD");

	for (const std::string dir : { "..", "../outside", "..\\..\\x", "C:\\Windows", "\\Windows", "/abs" }) {
		const auto result = write_tile_overrides(tileset_policy(false), map, std::vector<TileOverride> { grass(root / "good.blp", dir) });
		CHECK_MESSAGE(!result.has_value(), dir);
	}

	CHECK(file_count(root) == 1); // only good.blp: nothing escaped into the sandbox root, nothing inside the map
}

TEST_CASE("write_tile_overrides rejects two tiles that share a destination, before writing anything") {
	const fs::path root = make_scratch_dir("duplicate");
	const fs::path map = root / "map";
	write_all(root / "a.blp", "A");
	write_all(root / "b.blp", "B");
	write_all(map / "TerrainArt/Shared/Tile.blp", "ORIGINAL");

	// The same dir and file for two tiles, once with different letter case and slash style: one file on Windows.
	const std::vector<TileOverride> overrides = {
		{ "Lgrs", "TerrainArt\\Shared", "Tile", "blp", root / "a.blp" },
		{ "Ldrt", "terrainart/SHARED", "TILE", "blp", root / "b.blp" },
	};

	const auto result = write_tile_overrides(tileset_policy(false), map, overrides);

	REQUIRE_FALSE(result.has_value());
	CHECK(result.error().message.contains("Lgrs"));
	CHECK(result.error().message.contains("Ldrt"));
	CHECK(result.error().leftovers.empty());
	CHECK(read_all(map / "TerrainArt/Shared/Tile.blp") == "ORIGINAL");
	CHECK(file_count(map) == 1); // no temporary or set-aside file
}

// path::generic_string()/string() throw std::system_error on MSVC for characters the system code page cannot hold (Asana 1220842369265849).
TEST_CASE("write_tile_overrides works in a map folder with non-code-page characters") {
	const fs::path root = make_scratch_dir("unicode_ok");
	const fs::path map = root / fs::path(u8"地图_КАРТА");
	write_all(root / "a.blp", "A");
	write_all(root / "b.blp", "B");

	const std::vector<TileOverride> ok = { grass(root / "a.blp") };
	const auto result = write_tile_overrides(tileset_policy(false), map, ok);
	REQUIRE(result.has_value());
	CHECK(read_all(map / "TerrainArt/LordaeronSummer/Lords_Grass.blp") == "A");

	// The duplicate check still works with a non-code-page map folder. Only ASCII case folding (SHARED/shared) is exercised here.
	const std::vector<TileOverride> duplicate = {
		{ "Lgrs", "TerrainArt\\Shared", "Tile", "blp", root / "a.blp" },
		{ "Ldrt", "terrainart/SHARED", "TILE", "blp", root / "b.blp" },
	};
	const auto rejected = write_tile_overrides(tileset_policy(false), map, duplicate);
	REQUIRE_FALSE(rejected.has_value());
	CHECK(rejected.error().message.contains("Lgrs"));
	CHECK(rejected.error().message.contains("Ldrt"));
}

// This pins the UTF-8 message text; it only throws before the fix on machines whose ANSI code page is not UTF-8 (the test above is the
// one that reproduced the crash on the dev machine).
TEST_CASE("write_tile_overrides names a non-code-page source file in its error message as UTF-8") {
	const fs::path root = make_scratch_dir("unicode_copy_fail");
	const fs::path map = root / fs::path(u8"地图");
	const std::u8string expected_u8 = u8"缺失файл.blp";

	const auto result = write_tile_overrides(tileset_policy(false), map, std::vector<TileOverride> { grass(root / fs::path(expected_u8)) });

	REQUIRE_FALSE(result.has_value());
	CHECK(result.error().message.contains(std::string(expected_u8.begin(), expected_u8.end())));
	CHECK(result.error().leftovers.empty());
}

// More than the stock editor's 16 tiles (Asana 1220841004647015): the policy allows 64 (the 6-bit w3e corner field).
TEST_CASE("a tileset of more than 16 tiles remaps every slot and moves blight behind the last tile") {
	std::vector<std::string> many;
	for (int i = 0; i < 40; i++) {
		many.push_back(std::format("T{:03}", i));
	}
	CHECK(many.size() > 16);
	CHECK(many.size() <= tileset_policy(false).max_tiles);

	// Old tileset: the first five tiles. New: all 40 in reverse order.
	const std::vector<std::string> old_ids(many.begin(), many.begin() + 5);
	std::vector<std::string> new_ids(many.rbegin(), many.rend());
	const std::vector<int> remap = build_tile_remap(old_ids, new_ids, {});
	REQUIRE(remap.size() == 5);
	CHECK(remap[0] == 39);
	CHECK(remap[4] == 35);

	std::vector<std::uint8_t> corners = { 0, 4, 5, 6, 63 }; // tile, tile, blight, past blight, far out of range
	remap_ground_textures(corners, remap, new_ids.size());
	CHECK(corners == std::vector<std::uint8_t> { 39, 35, 40, 0, 0 });
}

// decode_tile_image and import_texture_file in the dialog both take the extension from texture_extension_of.
TEST_CASE("texture_extension_of folds case, drops the dot and survives non-code-page characters") {
	CHECK(texture_extension_of("Tile.BLP") == "blp");
	CHECK(texture_extension_of("a/b/Tile.Tga") == "tga");
	CHECK(texture_extension_of("noextension").empty());
	CHECK(texture_extension_of(fs::path(u8"地图/текстура.BLP")) == "blp");
	CHECK(texture_extension_of(fs::path(u8"x.бмп")) == "???");
}

TEST_CASE("write_tile_overrides takes a source file whose own extension is upper case") {
	const fs::path root = make_scratch_dir("upper_case_source");
	const fs::path map = root / "map";
	write_all(root / "MARBLE.BLP", "MARBLE");

	// The dialog lower-cases the extension it passes in; the stock tile path decides the written name.
	const auto result = write_tile_overrides(tileset_policy(false), map, std::vector<TileOverride> { grass(root / "MARBLE.BLP") });

	REQUIRE_MESSAGE(result.has_value(), result.error().message);
	CHECK(read_all(map / "TerrainArt/LordaeronSummer/Lords_Grass.blp") == "MARBLE");
}

TEST_CASE("write_tile_overrides copies a non-code-page source file into a non-code-page map folder") {
	const fs::path root = make_scratch_dir("unicode_source_ok");
	const fs::path map = root / fs::path(u8"地图_КАРТА");
	write_all(root / fs::path(u8"текстура.tga"), "TGA");
	TileOverride item = grass(root / fs::path(u8"текстура.tga"));
	item.extension = "tga";

	const auto result = write_tile_overrides(tileset_policy(false), map, std::vector<TileOverride> { item });

	REQUIRE_MESSAGE(result.has_value(), result.error().message);
	CHECK(read_all(map / "TerrainArt/LordaeronSummer/Lords_Grass.tga") == "TGA");
	CHECK(file_count(map) == 1);
}

TEST_CASE("a failed write reports no leftovers when the rollback restored everything") {
	const fs::path root = make_scratch_dir("no_leftovers");
	const fs::path map = root / "map";
	write_all(root / "good.blp", "GOOD");
	write_all(map / "TerrainArt/LordaeronSummer/Lords_Grass.blp", "ORIGINAL");
	const std::vector<TileOverride> overrides = {
		grass(root / "good.blp"),
		{ "Ldrt", "TerrainArt\\Other", "Lords_Dirt", "blp", root / "missing.blp" },
	};

	const auto result = write_tile_overrides(tileset_policy(false), map, overrides);

	REQUIRE_FALSE(result.has_value());
	CHECK(result.error().leftovers.empty());
}

TEST_CASE("is_inside_directory resolves .. and rejects siblings with the same prefix") {
	const fs::path root = make_scratch_dir("inside");
	const fs::path map = root / "map";
	fs::create_directories(map);

	CHECK(is_inside_directory(map, map / "a" / "b.blp"));
	CHECK(is_inside_directory(map, map / "a" / ".." / "b.blp"));
	CHECK_FALSE(is_inside_directory(map, map / ".." / "b.blp"));
	CHECK_FALSE(is_inside_directory(map, root / "map2" / "b.blp"));
}
