#include <doctest/doctest.h>
#define __STORMLIB_NO_STATIC_LINK__
#include "StormLib.h"

import std;
import AssetObfuscation;
import Doodads;
import GameCameras;
import Hierarchy;
import MapArchiveCheck;
import MapFormatCheck;
import MapInfo;
import MPQ;
import ProtectionPipeline;
import Regions;
import Terrain;
import Units;

namespace fs = std::filesystem;

namespace {
	fs::path make_scratch_dir(const std::string& name) {
		const fs::path dir = fs::temp_directory_path() / "hivewe_map_format_check_test" / name;
		std::error_code ec;
		fs::remove_all(dir, ec);
		fs::create_directories(dir, ec);
		return dir;
	}

	void write_all(const fs::path& path, const std::string& content) {
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		out << content;
	}

	std::string read_all(const fs::path& path) {
		std::ifstream in(path, std::ios::binary);
		return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
	}

	std::string le32(std::uint32_t value) {
		std::string bytes(4, '\0');
		for (int i = 0; i < 4; i++) {
			bytes[i] = static_cast<char>((value >> (8 * i)) & 0xFF);
		}
		return bytes;
	}

	std::string w3i_bytes(std::uint32_t version) {
		return le32(version) + std::string(600, '\0');
	}

	/// A w3i as HiveWE used to misread v39: 163 is where MapInfo::load's v33 layout reads the player count,
	/// and the stock 3.0 map has the unrelated bytes 0xFFFF4700 there. That was the shipped crash.
	std::string w3i_crash_bytes(std::uint32_t version) {
		std::string bytes = w3i_bytes(version);
		bytes.replace(163, 4, le32(0xFFFF4700));
		return bytes;
	}

	std::string doo_bytes(std::uint32_t version, std::uint32_t subversion = 11) {
		return "W3do" + le32(version) + le32(subversion) + std::string(32, '\0');
	}

	std::string w3e_bytes(std::uint32_t version) {
		return "W3E!" + le32(version) + std::string(32, '\0');
	}

	std::string wpm_bytes(std::uint32_t version) {
		return "MP3W" + le32(version) + std::string(8, '\0');
	}

	std::string count_file_bytes(std::uint32_t version) {
		return le32(version) + le32(0);
	}

	/// A complete map of accepted versions; each test then damages one file.
	fs::path make_valid_map(const std::string& name) {
		const fs::path dir = make_scratch_dir(name);
		write_all(dir / "war3map.w3i", w3i_bytes(33));
		write_all(dir / "war3map.w3e", w3e_bytes(12));
		write_all(dir / "war3map.doo", doo_bytes(8));
		write_all(dir / "war3mapUnits.doo", doo_bytes(8));
		write_all(dir / "war3map.w3r", count_file_bytes(5));
		write_all(dir / "war3map.w3c", count_file_bytes(0));
		write_all(dir / "war3map.w3s", count_file_bytes(3));
		write_all(dir / "war3map.wpm", wpm_bytes(0));
		return dir;
	}

	bool contains(const std::string& text, const std::string& part) {
		return text.find(part) != std::string::npos;
	}

	/// Above the highest accepted version: the file comes from a newer game, so HiveWE is what is behind.
	void check_refused_as_newer(const fs::path& dir, const std::string& file, const std::string& version) {
		const auto result = check_map_format_versions(dir);
		REQUIRE_FALSE(result.has_value());
		CHECK(contains(result.error(), file));
		CHECK(contains(result.error(), "version " + version));
		CHECK(contains(result.error(), "newer version of HiveWE"));
		CHECK(contains(result.error(), "3.0"));
	}

	/// At or below the highest accepted version: an old or odd format, so neither HiveWE nor 3.0 is to blame.
	void check_refused_as_unsupported(const fs::path& dir, const std::string& file, const std::string& version) {
		const auto result = check_map_format_versions(dir);
		REQUIRE_FALSE(result.has_value());
		CHECK(contains(result.error(), file));
		CHECK(contains(result.error(), "version " + version));
		CHECK(contains(result.error(), "does not support"));
		CHECK_FALSE(contains(result.error(), "newer version of HiveWE"));
		CHECK_FALSE(contains(result.error(), "3.0"));
	}

	/// A real .w3x holding the given files, with or without a (listfile) and never with (attributes). Built
	/// with SFileCreateArchive2, so unlike HiveWE::export_mpq it can really leave the listfile out.
	void make_archive(const fs::path& archive, const std::map<std::string, std::string>& files, const bool with_listfile = true) {
		const fs::path sources = make_scratch_dir("archive_sources");
		HANDLE handle = nullptr;
		// SFileCreateArchive always adds a (listfile), whatever its flags say, so leaving it out takes the "2" call.
		SFILE_CREATE_MPQ create {};
		create.cbSize = sizeof(SFILE_CREATE_MPQ);
		create.dwStreamFlags = STREAM_PROVIDER_FLAT | BASE_PROVIDER_FILE;
		create.dwFileFlags1 = with_listfile ? MPQ_FILE_DEFAULT_INTERNAL : 0;
		create.dwSectorSize = 0x1000;
		create.dwMaxFileCount = static_cast<DWORD>(files.size() + 2);
		REQUIRE(SFileCreateArchive2(archive.c_str(), &create, &handle));
		for (const auto& [name, content] : files) {
			const fs::path source = sources / name;
			write_all(source, content);
			REQUIRE(SFileAddFileEx(handle, source.c_str(), name.c_str(), MPQ_FILE_COMPRESS, MPQ_COMPRESSION_ZLIB, MPQ_COMPRESSION_NEXT_SAME));
		}
		REQUIRE(SFileCloseArchive(handle));
	}

	std::map<std::string, std::string> valid_map_files() {
		return {
			{ "war3map.w3i", w3i_bytes(33) },
			{ "war3map.w3e", w3e_bytes(12) },
			{ "war3map.doo", doo_bytes(8) },
			{ "war3mapUnits.doo", doo_bytes(8) },
			{ "war3map.w3r", count_file_bytes(5) },
			{ "war3map.w3c", count_file_bytes(0) },
			{ "war3map.w3s", count_file_bytes(3) },
			{ "war3map.wpm", wpm_bytes(0) },
		};
	}

	std::expected<void, std::string> check_archive(const fs::path& archive, const fs::path& scratch) {
		mpq::MPQ opened;
		REQUIRE(opened.open(archive, MPQ_OPEN_READ_ONLY));
		return check_archive_format_versions(opened, scratch);
	}

	using Snapshot = std::map<std::string, std::pair<std::size_t, fs::file_time_type>>;

	/// Every file below dir with its content hash and modification time, so added, removed, changed and
	/// rewritten-with-the-same-bytes files all show up.
	Snapshot snapshot(const fs::path& dir) {
		Snapshot result;
		for (const auto& entry : fs::recursive_directory_iterator(dir)) {
			if (entry.is_regular_file()) {
				const std::string relative = entry.path().lexically_relative(dir).string();
				result[relative] = { std::hash<std::string> {}(read_all(entry.path())), entry.last_write_time() };
			}
		}
		return result;
	}

	/// Points the readers at dir and restores the previous directory when the test ends.
	struct ScopedMapDirectory {
		fs::path previous = hierarchy.map_directory;
		explicit ScopedMapDirectory(const fs::path& dir) {
			hierarchy.map_directory = dir;
		}
		~ScopedMapDirectory() {
			hierarchy.map_directory = previous;
		}
	};

	/// Terrain's destructor calls GL, which does not exist here. Doodads and Units refuse before they
	/// look at the terrain, so one instance is kept alive for the process instead of being destroyed.
	const Terrain& terrain_without_gl() {
		static const Terrain* terrain = new Terrain();
		return *terrain;
	}

	/// What a w3i built by w3i_strict_bytes holds. Every field not listed here gets a distinct sentinel.
	struct W3iSpec {
		std::uint32_t version = 39;
		std::uint32_t editor = 6140;
		int players = 2;
		/// Unset follows the format's rule; set writes the player hud_skin on the wrong side of it.
		std::optional<bool> hud_skin = std::nullopt;
		std::uint32_t flags = 0x2180ABCD; // Known bits plus 0x800000, 0x1000000 and bit 29.
		std::uint32_t force_flags = 0x3B; // Includes 0x08, which HiveWE has no field for.
		bool random_tables = true;
		bool null_upgrade_ids = true;
		bool null_table_ids = true;
		/// For the expected output of an older file: what save() writes for fields that version does not have.
		std::uint32_t saved_from = 39;
		bool stop_after_forces = false;
		std::string loading_screen_model = "";
		std::optional<std::string> author = std::nullopt;
		std::optional<std::uint32_t> player_count = std::nullopt;
		std::optional<std::uint32_t> force_count = std::nullopt;
	};

	/// A v33 file with nothing the writer has ever had a problem with, for tests that vary one field.
	W3iSpec plain_v33_spec() {
		W3iSpec spec;
		spec.version = 33;
		spec.editor = 6116;
		spec.flags = 0x0000ABCD;
		spec.force_flags = 0x37;
		spec.random_tables = false;
		spec.null_upgrade_ids = false;
		spec.null_table_ids = false;
		return spec;
	}

	/// Sentinels are handed out in file order, so the strings have fixed values: name TRIGSTR_001, author
	/// TRIGSTR_002, description 003, suggested players 004, loading screen 005-008, prologue 009-012,
	/// sound environment 013, players from 014, then forces and tables.
	std::string w3i_strict_bytes(const W3iSpec& spec) {
		std::uint32_t next_u32 = 0x11110000;
		std::uint32_t next_f32 = 0x42000000;
		int next_string = 1;

		std::string bytes;
		const auto u32 = [&] { bytes += le32(next_u32++); };
		const auto u32s = [&](const int count) {
			for (int i = 0; i < count; i++) {
				u32();
			}
		};
		const auto f32s = [&](const int count) {
			for (int i = 0; i < count; i++) {
				bytes += le32(next_f32);
				next_f32 += 0x100;
			}
		};
		const auto string = [&] {
			bytes += std::format("TRIGSTR_{:03}", next_string++);
			bytes += '\0';
		};
		const auto id = [&](const bool null_id, const std::string_view plain) {
			bytes += null_id ? std::string(4, '\0') : std::string(plain);
		};

		// Sentinels are used up even when absent, so an older input and its expected output stay in step.
		const auto u32s_since = [&](const std::uint32_t first_version, const int count) {
			if (spec.version < first_version) {
				next_u32 += count;
			} else if (spec.saved_from < first_version) {
				next_u32 += count;
				bytes += std::string(count * 4, '\0');
			} else {
				u32s(count);
			}
		};

		const bool v39 = spec.version == 39;

		bytes += le32(spec.version);
		u32(); // map_version
		bytes += le32(spec.editor);
		if (v39) {
			u32s(4); // game version
		} else if (spec.version >= 28) {
			bytes += le32(2) + le32(0) + le32(3) + le32(22978);
		}
		string(); // name
		if (spec.author) {
			next_string++;
			bytes += *spec.author;
			bytes += '\0';
		} else {
			string();
		}
		string(); // description
		string(); // suggested players

		f32s(8); // camera bounds
		u32s(4); // complements
		u32s(2); // playable size
		bytes += le32(spec.flags);
		bytes += 'L'; // tileset

		u32(); // loading_screen_number
		if (v39) {
			u32(); // race_hud
		}
		if (spec.loading_screen_model.empty()) {
			string();
		} else {
			next_string++;
			bytes += spec.loading_screen_model;
			bytes += '\0';
		}
		string(); // loading screen text
		string(); // loading screen title
		string(); // loading screen subtitle
		u32(); // game_data_set
		string(); // prologue model
		string(); // prologue text
		string(); // prologue title
		string(); // prologue subtitle

		u32(); // fog_style
		f32s(3); // fog start, end, density
		u32(); // fog color
		if (v39) {
			f32s(5); // fog height start, height end, linear start, linear end, max opacity
			u32(); // fog_draw_over_sky
		}
		bytes += "RLlr"; // weather_id
		string(); // sound environment
		bytes += 'F'; // light tileset
		u32(); // water color

		if (spec.version >= 28) {
			bytes += le32(spec.saved_from >= 28 ? 1 : 0); // lua, which is a bool to HiveWE
		}
		u32s_since(31, 2); // supported_modes, game_data_version
		u32s_since(33, 3); // default, max, min camera distance
		if (v39) {
			u32s(7); // hd water
			u32(); // hd water override color
			u32(); // hd water envmap reflectivity
			u32(); // alpha tile minimap color
		}

		const bool hud_skin = spec.hud_skin.value_or(v39 && spec.editor >= 6140);
		bytes += le32(spec.player_count.value_or(static_cast<std::uint32_t>(spec.players)));
		for (int i = 0; i < spec.players; i++) {
			u32s(3); // id, type, race
			if (hud_skin) {
				u32();
			}
			u32(); // fixed_start
			string(); // name
			f32s(2); // start position
			u32s(2); // ally priorities
			u32s_since(31, 2); // enemy priorities
		}

		bytes += le32(spec.force_count.value_or(1));
		bytes += le32(spec.force_flags);
		u32(); // player mask
		string(); // force name

		if (spec.stop_after_forces) {
			return bytes;
		}

		bytes += le32(2);
		for (const bool null_id : { false, spec.null_upgrade_ids }) {
			u32(); // player flags
			id(null_id, "Rhme");
			u32s(2); // level, availability
		}
		bytes += le32(2);
		for (const bool null_id : { false, spec.null_upgrade_ids }) {
			u32(); // player flags
			id(null_id, "Ahbz");
		}

		if (spec.random_tables) {
			bytes += le32(1);
			u32(); // creation number
			string(); // name
			bytes += le32(2);
			u32s(2); // positions
			bytes += le32(2);
			for (const bool null_id : { false, spec.null_table_ids }) {
				u32(); // chance
				id(false, "hfoo");
				id(null_id, "hkni");
			}

			bytes += le32(1);
			u32(); // creation number
			string(); // name
			bytes += le32(1); // item sets
			bytes += le32(2); // items
			for (const bool null_id : { false, spec.null_table_ids }) {
				u32(); // chance
				id(null_id, "ratc");
			}
		} else {
			bytes += le32(0) + le32(0);
		}
		return bytes;
	}

	std::string w3i_v39_bytes(const std::uint32_t editor, const int players = 2) {
		W3iSpec spec;
		spec.editor = editor;
		spec.players = players;
		return w3i_strict_bytes(spec);
	}

	/// Loads bytes as war3map.w3i and returns what save() writes back. Throws what load() throws.
	std::string w3i_round_trip(const fs::path& dir, const std::string& bytes, const char tileset = 'L') {
		write_all(dir / "war3map.w3i", bytes);
		const ScopedMapDirectory open_map(dir);
		MapInfo info;
		info.load();
		info.save(tileset);
		return read_all(dir / "war3map.w3i");
	}

	/// Byte-exact, and on failure says where instead of printing two binary strings.
	void check_same_bytes(const std::string& actual, const std::string& expected) {
		const auto mismatch = std::ranges::mismatch(actual, expected);
		const auto offset = static_cast<std::size_t>(mismatch.in1 - actual.begin());
		CHECK_MESSAGE(
			actual == expected,
			std::format("sizes {} and {} expected, first difference at offset {}", actual.size(), expected.size(), offset)
		);
	}

	/// The tileset byte, which MapInfo::load discards and save() takes as an argument. Only for w3i >= 28.
	char w3i_tileset_byte(const std::string& bytes) {
		std::size_t position = 28;
		for (int i = 0; i < 4; i++) {
			position = bytes.find('\0', position) + 1;
		}
		return bytes.at(position + 60);
	}

	std::uint32_t w3i_version_of(const std::string& bytes) {
		std::uint32_t version = 0;
		std::memcpy(&version, bytes.data(), std::min<std::size_t>(bytes.size(), 4));
		return version;
	}

	template <typename Load>
	void check_reader_refuses(Load&& load, const std::string& file, const std::string& version) {
		try {
			load();
			FAIL("the reader did not refuse ", file, " version ", version);
		} catch (const std::runtime_error& e) {
			CHECK(contains(e.what(), file));
			CHECK(contains(e.what(), "version " + version));
		} catch (const std::exception& e) {
			FAIL("expected std::runtime_error, got: ", std::string(e.what()));
		}
	}
}

TEST_CASE("check_map_format_versions accepts a complete map of accepted versions") {
	CHECK(check_map_format_versions(make_valid_map("valid")).has_value());
}

TEST_CASE("check_map_format_versions accepts and refuses war3map.w3i versions") {
	const fs::path dir = make_valid_map("w3i");
	for (const std::uint32_t version : { 15u, 18u, 25u, 26u, 28u, 31u, 32u, 33u, 39u }) {
		CAPTURE(version);
		write_all(dir / "war3map.w3i", w3i_bytes(version));
		CHECK(check_map_format_versions(dir).has_value());
	}
	for (const std::uint32_t version : { 0u, 8u, 10u, 11u, 23u, 24u, 27u, 34u, 35u, 36u, 37u, 38u }) {
		CAPTURE(version);
		write_all(dir / "war3map.w3i", w3i_bytes(version));
		check_refused_as_unsupported(dir, "war3map.w3i", std::to_string(version));
	}
	for (const std::uint32_t version : { 40u, 0xFFFFFFFFu }) {
		CAPTURE(version);
		write_all(dir / "war3map.w3i", w3i_bytes(version));
		check_refused_as_newer(dir, "war3map.w3i", std::to_string(version));
	}
}

TEST_CASE("check_map_format_versions accepts and refuses war3map.doo versions, ignoring the subversion") {
	const fs::path dir = make_valid_map("doo");
	for (const std::uint32_t version : { 7u, 8u, 12u, 13u }) {
		for (const std::uint32_t subversion : { 0u, 9u, 11u, 99u }) {
			CAPTURE(version);
			CAPTURE(subversion);
			write_all(dir / "war3map.doo", doo_bytes(version, subversion));
			CHECK(check_map_format_versions(dir).has_value());
		}
	}
	write_all(dir / "war3map.doo", doo_bytes(6));
	check_refused_as_unsupported(dir, "war3map.doo", "6");
	for (const std::uint32_t version : { 9u, 10u, 11u }) {
		CAPTURE(version);
		write_all(dir / "war3map.doo", doo_bytes(version));
		check_refused_as_unsupported(dir, "war3map.doo", std::to_string(version));
	}
	write_all(dir / "war3map.doo", doo_bytes(14));
	check_refused_as_newer(dir, "war3map.doo", "14");
}

TEST_CASE("check_map_format_versions accepts and refuses war3mapUnits.doo versions and subversions") {
	const fs::path dir = make_valid_map("units");
	for (const std::uint32_t version : { 7u, 8u, 12u, 13u }) {
		for (const std::uint32_t subversion : { 9u, 11u }) {
			CAPTURE(version);
			CAPTURE(subversion);
			write_all(dir / "war3mapUnits.doo", doo_bytes(version, subversion));
			CHECK(check_map_format_versions(dir).has_value());
		}
	}
	write_all(dir / "war3mapUnits.doo", doo_bytes(6, 9));
	check_refused_as_unsupported(dir, "war3mapUnits.doo", "6");
	for (const std::uint32_t version : { 9u, 10u, 11u }) {
		CAPTURE(version);
		write_all(dir / "war3mapUnits.doo", doo_bytes(version, 9));
		check_refused_as_unsupported(dir, "war3mapUnits.doo", std::to_string(version));
	}
	write_all(dir / "war3mapUnits.doo", doo_bytes(14, 9));
	check_refused_as_newer(dir, "war3mapUnits.doo", "14");
	// "version N" is also found inside "subversion N", so these name the subversion as the check does.
	for (const std::uint32_t subversion : { 7u, 8u, 10u }) {
		CAPTURE(subversion);
		write_all(dir / "war3mapUnits.doo", doo_bytes(8, subversion));
		check_refused_as_unsupported(dir, "war3mapUnits.doo", std::to_string(subversion));
		CHECK(contains(check_map_format_versions(dir).error(), "subversion " + std::to_string(subversion)));
	}
	write_all(dir / "war3mapUnits.doo", doo_bytes(8, 12));
	check_refused_as_newer(dir, "war3mapUnits.doo", "12");
	CHECK(contains(check_map_format_versions(dir).error(), "subversion 12"));
	write_all(dir / "war3mapUnits.doo", doo_bytes(13, 10));
	check_refused_as_unsupported(dir, "war3mapUnits.doo", "10");
	CHECK(contains(check_map_format_versions(dir).error(), "subversion 10"));
}

TEST_CASE("check_map_format_versions accepts and refuses the remaining files") {
	struct Row {
		std::string file;
		std::function<std::string(std::uint32_t)> bytes;
		std::vector<std::uint32_t> accepted;
		std::vector<std::uint32_t> unsupported;
		std::vector<std::uint32_t> newer;
	};
	const std::vector<Row> rows = {
		{ "war3map.w3r", count_file_bytes, { 2, 3, 5 }, { 0, 1, 4 }, { 6, 7 } },
		{ "war3map.w3c", count_file_bytes, { 0 }, {}, { 1, 2, 3 } },
		{ "war3map.w3e", w3e_bytes, { 11, 12 }, { 0, 10 }, { 13 } },
		{ "war3map.w3s", count_file_bytes, { 1, 2, 3 }, { 0 }, { 4 } },
		{ "war3map.wpm", wpm_bytes, { 0 }, {}, { 1, 2 } },
	};
	for (const Row& row : rows) {
		CAPTURE(row.file);
		const fs::path dir = make_valid_map("remaining_" + row.file);
		for (const std::uint32_t version : row.accepted) {
			CAPTURE(version);
			write_all(dir / row.file, row.bytes(version));
			CHECK(check_map_format_versions(dir).has_value());
		}
		for (const std::uint32_t version : row.unsupported) {
			CAPTURE(version);
			write_all(dir / row.file, row.bytes(version));
			check_refused_as_unsupported(dir, row.file, std::to_string(version));
		}
		for (const std::uint32_t version : row.newer) {
			CAPTURE(version);
			write_all(dir / row.file, row.bytes(version));
			check_refused_as_newer(dir, row.file, std::to_string(version));
		}
	}
}

TEST_CASE("check_map_format_versions refuses a missing required file and allows a missing optional file") {
	for (const std::string file : { "war3map.w3i", "war3map.w3e", "war3map.doo" }) {
		CAPTURE(file);
		const fs::path dir = make_valid_map("missing_required");
		fs::remove(dir / file);
		const auto result = check_map_format_versions(dir);
		REQUIRE_FALSE(result.has_value());
		CHECK(contains(result.error(), file));
		CHECK(contains(result.error(), "missing"));
	}
	for (const std::string file : { "war3mapUnits.doo", "war3map.w3r", "war3map.w3c", "war3map.w3s", "war3map.wpm" }) {
		CAPTURE(file);
		const fs::path dir = make_valid_map("missing_optional");
		fs::remove(dir / file);
		CHECK(check_map_format_versions(dir).has_value());
	}
}

TEST_CASE("check_map_format_versions refuses a directory that holds no map") {
	const fs::path dir = make_scratch_dir("empty");
	CHECK_FALSE(check_map_format_versions(dir).has_value());
	CHECK_FALSE(check_map_format_versions(dir / "does_not_exist").has_value());
}

TEST_CASE("check_map_format_versions refuses a file too short for its header") {
	struct Row {
		std::string file;
		std::string header;
	};
	const std::vector<Row> rows = {
		{ "war3map.w3i", le32(33) },
		{ "war3map.w3e", "W3E!" + le32(12) },
		{ "war3map.doo", "W3do" + le32(8) + le32(11) },
		{ "war3mapUnits.doo", "W3do" + le32(8) + le32(11) },
		{ "war3map.w3r", le32(5) },
		{ "war3map.w3c", le32(0) },
		{ "war3map.w3s", le32(3) },
		{ "war3map.wpm", "MP3W" + le32(0) },
	};
	for (const Row& row : rows) {
		const fs::path dir = make_valid_map("short");
		for (std::size_t size = 0; size < row.header.size(); size++) {
			CAPTURE(row.file);
			CAPTURE(size);
			write_all(dir / row.file, row.header.substr(0, size));
			const auto result = check_map_format_versions(dir);
			REQUIRE_FALSE(result.has_value());
			CHECK(contains(result.error(), row.file));
			CHECK(contains(result.error(), "too short"));
		}
		// Exactly the header is enough for this check; a truncated body is the reader's problem.
		write_all(dir / row.file, row.header);
		CAPTURE(row.file);
		CHECK(check_map_format_versions(dir).has_value());
	}
}

TEST_CASE("check_map_format_versions refuses a wrong magic where loading on would do harm") {
	// Map::load ignores these readers returning false. Empty terrain is indexed by interpolated_height, and
	// a 0 by 0 pathing map would overwrite the real war3map.wpm with an empty one on the next save.
	for (const std::string file : { "war3map.w3e", "war3map.wpm" }) {
		CAPTURE(file);
		const fs::path dir = make_valid_map("wrong_magic_refused");
		for (const std::string bytes : { "XXXX" + le32(12), std::string("XXXX") }) {
			write_all(dir / file, bytes);
			const auto result = check_map_format_versions(dir);
			REQUIRE_FALSE(result.has_value());
			CHECK(contains(result.error(), file));
			CHECK(contains(result.error(), "magic"));
			CHECK_FALSE(contains(result.error(), "newer version of HiveWE"));
		}
	}
}

TEST_CASE("check_map_format_versions does not newly refuse a wrong magic where the reader copes") {
	// Doodads returns false with empty vectors, which every later use of them handles.
	const fs::path doo_dir = make_valid_map("wrong_magic_doo");
	write_all(doo_dir / "war3map.doo", "XXXX" + le32(999));
	CHECK(check_map_format_versions(doo_dir).has_value());
	write_all(doo_dir / "war3map.doo", "XXXX");
	CHECK(check_map_format_versions(doo_dir).has_value());

	// Units keeps reading past a wrong magic, so its version is still checked.
	const fs::path dir = make_valid_map("wrong_magic_units");
	write_all(dir / "war3mapUnits.doo", "XXXX" + le32(8) + le32(11));
	CHECK(check_map_format_versions(dir).has_value());
	write_all(dir / "war3mapUnits.doo", "XXXX" + le32(14) + le32(11));
	check_refused_as_newer(dir, "war3mapUnits.doo", "14");
}

TEST_CASE("check_map_format_versions reports every failing file") {
	const fs::path dir = make_valid_map("all_failures");
	write_all(dir / "war3map.w3i", w3i_bytes(40));
	write_all(dir / "war3map.doo", doo_bytes(14));
	write_all(dir / "war3mapUnits.doo", doo_bytes(15));
	write_all(dir / "war3map.w3r", count_file_bytes(7));
	write_all(dir / "war3map.w3c", count_file_bytes(3));
	write_all(dir / "war3map.w3s", "");
	const auto result = check_map_format_versions(dir);
	REQUIRE_FALSE(result.has_value());
	for (const std::string part : { "war3map.w3i", "version 40", "war3map.doo", "version 14", "war3mapUnits.doo", "version 15", "war3map.w3r", "version 7",
									"war3map.w3c", "version 3", "war3map.w3s" }) {
		CAPTURE(part);
		CHECK(contains(result.error(), part));
	}
}

TEST_CASE("a damaged map is not told to get a newer HiveWE") {
	const fs::path dir = make_valid_map("damaged_wording");
	fs::remove(dir / "war3map.doo");
	const auto result = check_map_format_versions(dir);
	REQUIRE_FALSE(result.has_value());
	CHECK_FALSE(contains(result.error(), "newer version of HiveWE"));
}

TEST_CASE("check_map_format_versions passes the bundled test map and leaves it untouched") {
	const fs::path dir = TEST_MAP_DIR;
	REQUIRE(fs::exists(dir / "war3map.w3i"));
	const Snapshot before = snapshot(dir);
	const auto result = check_map_format_versions(dir);
	CHECK_MESSAGE(result.has_value(), (result.has_value() ? std::string() : result.error()));
	CHECK(snapshot(dir) == before);
}

TEST_CASE("a refusal writes nothing and does not touch hierarchy.map_directory") {
	const fs::path dir = make_valid_map("no_write");
	write_all(dir / "war3map.w3i", w3i_bytes(39));
	write_all(dir / "war3map.doo", doo_bytes(13));
	write_all(dir / "war3map.w3r", count_file_bytes(7));
	const Snapshot before = snapshot(dir);

	const ScopedMapDirectory open_map("some/open/map");
	CHECK_FALSE(check_map_format_versions(dir).has_value());
	CHECK(hierarchy.map_directory == fs::path("some/open/map"));
	CHECK(snapshot(dir) == before);
}

TEST_CASE("a map with both a too-new and a too-old file blames the newer format") {
	const fs::path dir = make_valid_map("mixed_wording");
	write_all(dir / "war3map.w3i", w3i_bytes(40));
	write_all(dir / "war3map.doo", doo_bytes(6));
	const auto result = check_map_format_versions(dir);
	REQUIRE_FALSE(result.has_value());
	CHECK(contains(result.error(), "newer version of HiveWE"));
	CHECK(contains(result.error(), "war3map.doo is version 6, which HiveWE does not support"));
}

TEST_CASE("require_accepted words the exception like the preflight") {
	CHECK_THROWS_WITH_AS(
		map_format::require_accepted("war3map.w3i", map_format::w3i_versions, 40),
		doctest::Contains("newer version of HiveWE"),
		std::runtime_error
	);
	try {
		map_format::require_accepted("war3map.w3i", map_format::w3i_versions, 27);
		FAIL("version 27 was accepted");
	} catch (const std::runtime_error& e) {
		CHECK(contains(e.what(), "does not support"));
		CHECK_FALSE(contains(e.what(), "newer version of HiveWE"));
	}
}

TEST_CASE("check_archive_format_versions accepts an archive of accepted versions, with or without a listfile") {
	for (const bool with_listfile : { true, false }) {
		CAPTURE(with_listfile);
		const fs::path dir = make_scratch_dir("archive_accepted");
		const fs::path archive = dir / "Foo.w3x";
		make_archive(archive, valid_map_files(), with_listfile);
		{
			mpq::MPQ opened;
			REQUIRE(opened.open(archive, MPQ_OPEN_READ_ONLY));
			CHECK(opened.file_exists("(listfile)") == with_listfile);
		}
		const fs::path scratch = make_scratch_dir("archive_accepted_scratch");
		const auto result = check_archive(archive, scratch);
		CHECK_MESSAGE(result.has_value(), (result.has_value() ? std::string() : result.error()));
	}
}

TEST_CASE("check_archive_format_versions refuses a file that is in the archive but cannot be extracted") {
	const fs::path dir = make_scratch_dir("archive_extract_failure");
	make_archive(dir / "Foo.w3x", valid_map_files());
	const fs::path scratch = make_scratch_dir("archive_extract_failure_scratch");
	fs::create_directories(scratch / "war3map.w3i"); // A directory in the way makes the extraction fail.

	const auto result = check_archive(dir / "Foo.w3x", scratch);
	REQUIRE_FALSE(result.has_value());
	CHECK(contains(result.error(), "war3map.w3i could not be read from the archive"));
}

TEST_CASE("a read-only archive handle can do everything load_mpq asks of it") {
	const fs::path dir = make_scratch_dir("archive_read_only");
	const fs::path archive = dir / "Foo.w3x";
	const auto files = valid_map_files();
	make_archive(archive, files);
	const Snapshot source_before = snapshot(dir);

	const fs::path target = make_scratch_dir("archive_read_only_target");
	{
		mpq::MPQ opened;
		REQUIRE(opened.open(archive, MPQ_OPEN_READ_ONLY));
		CHECK(check_archive_format_versions(opened, make_scratch_dir("archive_read_only_scratch")).has_value());
		REQUIRE(opened.unpack(target));
		CHECK(opened.extract_file("war3map.w3i", target / "by_name.w3i"));
	}
	for (const auto& [name, content] : files) {
		CAPTURE(name);
		CHECK(read_all(target / name) == content);
	}
	CHECK(read_all(target / "by_name.w3i") == files.at("war3map.w3i"));
	CHECK_FALSE(fs::exists(target / "(listfile)"));
	CHECK(snapshot(dir) == source_before);
}

TEST_CASE("a stale file in the unpack folder is refused after the archive passed, without claiming nothing changed") {
	// load_mpq checks the archive first, but the folder it unpacks into can hold what the archive lacks.
	const fs::path dir = make_scratch_dir("archive_stale_target");
	auto files = valid_map_files();
	files.erase("war3map.w3c");
	make_archive(dir / "Foo.w3x", files);
	CHECK(check_archive(dir / "Foo.w3x", make_scratch_dir("archive_stale_target_scratch")).has_value());

	const fs::path target = dir / "Foo";
	fs::create_directories(target);
	write_all(target / "war3map.w3c", count_file_bytes(3));
	{
		mpq::MPQ opened;
		REQUIRE(opened.open(dir / "Foo.w3x", MPQ_OPEN_READ_ONLY));
		REQUIRE(opened.unpack(target));
	}

	const auto result = check_map_format_versions(target);
	REQUIRE_FALSE(result.has_value());
	CHECK(contains(result.error(), "war3map.w3c"));
	CHECK_FALSE(contains(result.error(), "not changed"));
}

TEST_CASE("no refusal claims the map was not changed, since the check cannot know what its caller did") {
	const fs::path dir = make_valid_map("no_change_claim");
	write_all(dir / "war3map.w3i", w3i_bytes(40));
	CHECK_FALSE(contains(check_map_format_versions(dir).error(), "not changed"));

	make_archive(dir / "Foo.w3x", valid_map_files());
	const fs::path scratch = make_scratch_dir("no_change_claim_scratch");
	fs::create_directories(scratch / "war3map.w3i");
	CHECK_FALSE(contains(check_archive(dir / "Foo.w3x", scratch).error(), "not changed"));
}

TEST_CASE("checked_map_files lists the eight files the check reads") {
	const auto files = checked_map_files();
	CHECK(files.size() == 8);
	const fs::path dir = make_valid_map("checked_files");
	for (const std::string_view file : files) {
		CAPTURE(file);
		CHECK(fs::exists(dir / file));
	}
}

TEST_CASE("check_archive_format_versions refuses a bad version in each checked file and writes only to its scratch folder") {
	const std::map<std::string, std::string> bad_files = {
		{ "war3map.w3i", w3i_bytes(40) },
		{ "war3map.w3e", w3e_bytes(13) },
		{ "war3map.doo", doo_bytes(14) },
		{ "war3mapUnits.doo", doo_bytes(14) },
		{ "war3map.w3r", count_file_bytes(7) },
		{ "war3map.w3c", count_file_bytes(3) },
		{ "war3map.w3s", count_file_bytes(4) },
		{ "war3map.wpm", wpm_bytes(1) },
	};
	for (const auto& [file, bytes] : bad_files) {
		CAPTURE(file);
		// Foo/ next to Foo.w3x is the pairing export_mpq produces: it must survive a refusal untouched.
		const fs::path dir = make_scratch_dir("archive_refused");
		const fs::path archive = dir / "Foo.w3x";
		auto files = valid_map_files();
		files[file] = bytes;
		make_archive(archive, files);
		fs::create_directories(dir / "Foo");
		write_all(dir / "Foo" / "war3map.w3i", "the user's existing map");

		const Snapshot before = snapshot(dir);
		const fs::path scratch = make_scratch_dir("archive_refused_scratch");
		const auto result = check_archive(archive, scratch);
		REQUIRE_FALSE(result.has_value());
		CHECK(contains(result.error(), file));
		CHECK(contains(result.error(), "newer version of HiveWE"));
		CHECK(snapshot(dir) == before);
	}
}

TEST_CASE("check_archive_format_versions matches the folder check on absent files") {
	const fs::path dir = make_scratch_dir("archive_absent");

	auto no_doodads = valid_map_files();
	no_doodads.erase("war3map.doo");
	make_archive(dir / "required.w3x", no_doodads);
	const auto required = check_archive(dir / "required.w3x", make_scratch_dir("archive_absent_required"));
	REQUIRE_FALSE(required.has_value());
	CHECK(contains(required.error(), "war3map.doo"));
	CHECK(contains(required.error(), "missing"));

	auto no_regions = valid_map_files();
	no_regions.erase("war3map.w3r");
	make_archive(dir / "optional.w3x", no_regions);
	CHECK(check_archive(dir / "optional.w3x", make_scratch_dir("archive_absent_optional")).has_value());
}

TEST_CASE("is_open_map_folder recognizes the open map's folder however it is spelled") {
	const fs::path dir = make_scratch_dir("open_map_folder");
	const fs::path open_map = dir / "Foo";
	fs::create_directories(open_map);
	fs::create_directories(dir / "Bar");

	CHECK(is_open_map_folder(open_map, open_map));
	CHECK(is_open_map_folder(dir / "Bar" / ".." / "Foo", fs::absolute(open_map) / ""));
	CHECK_FALSE(is_open_map_folder(dir / "Bar", open_map));

	// A target that does not exist yet cannot be the open map, and a new map has no folder at all.
	CHECK_FALSE(is_open_map_folder(dir / "NotCreated", open_map));
	CHECK_FALSE(is_open_map_folder(open_map, ""));
}

TEST_CASE("MapInfo::load refuses a newer war3map.w3i instead of allocating from misread bytes") {
	const fs::path dir = make_scratch_dir("reader_w3i_newer");
	write_all(dir / "war3map.w3i", w3i_crash_bytes(40));
	const ScopedMapDirectory open_map(dir);
	MapInfo info;
	check_reader_refuses([&] { info.load(); }, "war3map.w3i", "40");
}

TEST_CASE("MapInfo::load still reads every accepted version") {
	const fs::path dir = make_scratch_dir("reader_w3i_accepted");
	const ScopedMapDirectory open_map(dir);
	for (const std::uint32_t version : map_format::w3i_versions) {
		CAPTURE(version);
		// v39 is parsed strictly, so the zero filler of w3i_bytes is not a valid v39 file.
		write_all(dir / "war3map.w3i", version == 39 ? w3i_v39_bytes(6140) : w3i_bytes(version));
		MapInfo info;
		CHECK_NOTHROW(info.load());
	}
}

TEST_CASE("Regions::load refuses an unknown war3map.w3r version and reads accepted ones") {
	const fs::path dir = make_scratch_dir("reader_w3r");
	const ScopedMapDirectory open_map(dir);
	write_all(dir / "war3map.w3r", count_file_bytes(7));
	Regions regions;
	check_reader_refuses([&] { regions.load(0.f, 0.f); }, "war3map.w3r", "7");
	for (const std::uint32_t version : map_format::w3r_versions) {
		CAPTURE(version);
		write_all(dir / "war3map.w3r", count_file_bytes(version));
		CHECK_NOTHROW(regions.load(0.f, 0.f));
	}
}

TEST_CASE("GameCameras::load refuses an unknown war3map.w3c version and reads accepted ones") {
	const fs::path dir = make_scratch_dir("reader_w3c");
	const ScopedMapDirectory open_map(dir);
	write_all(dir / "war3map.w3c", count_file_bytes(3));
	GameCameras cameras;
	check_reader_refuses([&] { cameras.load(2, 0, 0.f, 0.f); }, "war3map.w3c", "3");
	write_all(dir / "war3map.w3c", count_file_bytes(0));
	CHECK_NOTHROW(cameras.load(2, 0, 0.f, 0.f));
}

TEST_CASE("Doodads::load refuses an unknown war3map.doo version") {
	const fs::path dir = make_scratch_dir("reader_doo");
	const ScopedMapDirectory open_map(dir);
	write_all(dir / "war3map.doo", doo_bytes(14));
	Doodads doodads;
	MapInfo info;
	check_reader_refuses([&] { doodads.load(terrain_without_gl(), info); }, "war3map.doo", "14");
}

TEST_CASE("Units::load refuses an unknown war3mapUnits.doo version or subversion") {
	const fs::path dir = make_scratch_dir("reader_units");
	const ScopedMapDirectory open_map(dir);
	Units units;
	MapInfo info;
	write_all(dir / "war3mapUnits.doo", doo_bytes(14, 11));
	check_reader_refuses([&] { units.load(terrain_without_gl(), info); }, "war3mapUnits.doo", "14");
	write_all(dir / "war3mapUnits.doo", doo_bytes(8, 10));
	check_reader_refuses([&] { units.load(terrain_without_gl(), info); }, "war3mapUnits.doo", "10");
}

TEST_CASE("rewrite_map_info_references round-trips a v39 war3map.w3i") {
	const fs::path dir = make_scratch_dir("protector_v39_rewrite");
	W3iSpec spec;
	spec.loading_screen_model = "war3mapImported/Custom.mdx";
	write_all(dir / "war3map.w3i", w3i_strict_bytes(spec));
	write_all(dir / "war3map.w3e", "W3E!" + le32(12) + "L");

	RenameCandidate candidate;
	candidate.original_relative_path = "war3mapImported/Custom.mdx";
	candidate.new_relative_path = "a3f9c1e2.mdx";
	candidate.match_key = asset_match_key("war3mapImported/Custom.mdx");

	const ScopedMapDirectory open_map("some/open/map");
	const AssetObfuscationResult result = rewrite_map_info_references(dir, { candidate });
	CHECK_MESSAGE(result.success, result.error);
	CHECK(hierarchy.map_directory == fs::path("some/open/map"));

	spec.loading_screen_model = "a3f9c1e2.mdx";
	check_same_bytes(read_all(dir / "war3map.w3i"), w3i_strict_bytes(spec));
}

TEST_CASE("sanitize_metadata round-trips a v39 war3map.w3i") {
	const fs::path source = make_scratch_dir("protector_v39_sanitize_source");
	const fs::path temp = make_scratch_dir("protector_v39_sanitize_temp");
	W3iSpec spec;
	write_all(source / "war3map.w3i", w3i_strict_bytes(spec));
	write_all(source / "war3map.w3e", "W3E!" + le32(12) + "L");

	ProtectionOptions options;
	options.clear_author = true;
	const SyncSaveResult result = prepare_source_path(source, temp, options);
	CHECK_MESSAGE(result.success, result.error);

	spec.author = "";
	check_same_bytes(read_all(temp / "war3map.w3i"), w3i_strict_bytes(spec));
}

TEST_CASE("inline_map_info_trigger_strings keeps a v39 war3map.w3i at v39") {
	const fs::path dir = make_scratch_dir("protector_v39_inline");
	write_all(dir / "war3map.w3i", w3i_v39_bytes(6140));
	write_all(dir / "war3map.w3e", "W3E!" + le32(12) + "L");
	std::string strings;
	for (int i = 1; i <= 40; i++) {
		strings += std::format("STRING {}\n{{\nText {}\n}}\n\n", i, i);
	}
	write_all(dir / "war3map.wts", strings);

	ProtectionOptions options;
	options.strip_trigger_strings = true;
	options.obfuscate_asset_paths = false;
	const PackResult result = run_async_pack(dir, make_scratch_dir("protector_v39_inline_out") / "Out.w3x", options);
	CHECK_MESSAGE(result.success, result.error);

	CHECK(w3i_version_of(read_all(dir / "war3map.w3i")) == 39);
	const ScopedMapDirectory open_map(dir);
	MapInfo info;
	REQUIRE_NOTHROW(info.load());
	CHECK(info.name == "Text 1");
	CHECK(info.editor_version == 6140);
	REQUIRE(info.players.size() == 2);
	CHECK(info.players[1].name == "Text 15");
	REQUIRE(info.forces.size() == 1);
	CHECK(info.forces[0].name == "Text 16");
}

TEST_CASE("run_asset_obfuscation refuses a v39 war3map.w3i and rewrites nothing") {
	const fs::path dir = make_scratch_dir("protector_v39_obfuscation");
	write_all(dir / "war3map.w3i", w3i_v39_bytes(6140));
	const Snapshot before = snapshot(dir);

	const AssetObfuscationResult result = run_asset_obfuscation(dir);
	CHECK_FALSE(result.success);
	CHECK(contains(result.error, "war3map.w3i"));
	CHECK(contains(result.error, "version 39"));
	CHECK(snapshot(dir) == before);

	write_all(dir / "war3map.w3i", w3i_bytes(40));
	const AssetObfuscationResult newer = run_asset_obfuscation(dir);
	CHECK_FALSE(newer.success);
	CHECK(contains(newer.error, "version 40"));
}

TEST_CASE("run_asset_obfuscation does not refuse a v33 map or a map without war3map.w3i") {
	// Nothing to rename in either, so neither gets as far as the game files, which do not exist here.
	const fs::path dir = make_scratch_dir("protector_v33_obfuscation");
	const AssetObfuscationResult no_w3i = run_asset_obfuscation(dir);
	CHECK_MESSAGE(no_w3i.success, no_w3i.error);

	write_all(dir / "war3map.w3i", w3i_bytes(33));
	const AssetObfuscationResult v33 = run_asset_obfuscation(dir);
	CHECK_MESSAGE(v33.success, v33.error);
}

TEST_CASE("MapInfo reads and saves a v39 war3map.w3i with editors on both sides of the hud_skin gate") {
	const fs::path dir = make_scratch_dir("w3i_v39_gate");
	for (const std::uint32_t editor : { 6117u, 6127u, 6140u, 7000u, 7003u }) {
		CAPTURE(editor);
		const std::string bytes = w3i_v39_bytes(editor);
		write_all(dir / "war3map.w3i", bytes);
		{
			const ScopedMapDirectory open_map(dir);
			MapInfo info;
			REQUIRE_NOTHROW(info.load());
			CHECK(info.editor_version == static_cast<int>(editor));
			CHECK(info.name == "TRIGSTR_001");
			CHECK(info.author == "TRIGSTR_002");
			CHECK(info.description == "TRIGSTR_003");
			CHECK(info.suggested_players == "TRIGSTR_004");
			CHECK(info.loading_screen_model == "TRIGSTR_005");
			CHECK(info.loading_screen_subtitle == "TRIGSTR_008");
			CHECK(info.prologue_screen_model == "TRIGSTR_009");
			CHECK(info.prologue_subtitle == "TRIGSTR_012");
			CHECK(info.custom_sound_environment == "TRIGSTR_013");
			REQUIRE(info.players.size() == 2);
			CHECK(info.players[0].name == "TRIGSTR_014");
			CHECK(info.players[1].name == "TRIGSTR_015");
			REQUIRE(info.forces.size() == 1);
			CHECK(info.forces[0].name == "TRIGSTR_016");
			REQUIRE(info.random_unit_tables.size() == 1);
			CHECK(info.random_unit_tables[0].name == "TRIGSTR_017");
			REQUIRE(info.random_item_tables.size() == 1);
			CHECK(info.random_item_tables[0].name == "TRIGSTR_018");
			CHECK(info.custom_light_tileset == 'F');
			CHECK(std::memcmp(&info.weather_id, "RLlr", 4) == 0);
		}
		check_same_bytes(w3i_round_trip(dir, bytes), bytes);
	}
}

TEST_CASE("MapInfo reads every new v39 field from its own place in the file") {
	const fs::path dir = make_scratch_dir("w3i_v39_fields");
	write_all(dir / "war3map.w3i", w3i_v39_bytes(6140));
	const ScopedMapDirectory open_map(dir);
	MapInfo info;
	REQUIRE_NOTHROW(info.load());

	// The sentinels w3i_strict_bytes hands out, counted in file order. Round trips cannot catch swapped fields.
	const auto sentinel = [](const std::uint32_t n) { return 0x11110000u + n; };
	const auto f32 = [](const std::uint32_t n) { return std::bit_cast<float>(0x42000000u + n * 0x100u); };
	const auto color = [](const auto value) { return std::bit_cast<std::uint32_t>(value); };

	CHECK(info.format_version == 39);
	CHECK(info.map_version == static_cast<int>(sentinel(0)));
	CHECK(info.game_version_major == static_cast<int>(sentinel(1)));
	CHECK(info.game_version_build == static_cast<int>(sentinel(4)));
	CHECK(info.all_flags == 0x2180ABCD);
	CHECK(info.loading_screen_number == static_cast<int>(sentinel(11)));
	CHECK(info.race_hud == sentinel(12));
	CHECK(info.game_data_set == static_cast<int>(sentinel(13)));
	CHECK(info.fog_style == static_cast<int>(sentinel(14)));
	CHECK(info.fog_density == f32(10));
	CHECK(color(info.fog_color) == sentinel(15));
	CHECK(info.fog_height_start == f32(11));
	CHECK(info.fog_height_end == f32(12));
	CHECK(info.fog_linear_start == f32(13));
	CHECK(info.fog_linear_end == f32(14));
	CHECK(info.fog_max_opacity == f32(15));
	CHECK(info.fog_draw_over_sky == static_cast<int>(sentinel(16)));
	CHECK(color(info.water_color) == sentinel(17));
	CHECK(info.supported_modes == sentinel(18));
	CHECK(info.game_data_version == sentinel(19));
	CHECK(info.default_cam_distance == sentinel(20));
	CHECK(info.min_cam_distance == sentinel(22));
	CHECK(info.hd_water_min_opacity == static_cast<int>(sentinel(23)));
	CHECK(info.hd_water_max_opacity == static_cast<int>(sentinel(24)));
	CHECK(info.hd_water_reflectivity == static_cast<int>(sentinel(25)));
	CHECK(info.hd_water_emissivity == static_cast<int>(sentinel(26)));
	CHECK(info.hd_water_edge_softness == static_cast<int>(sentinel(27)));
	CHECK(info.hd_water_waves_vertex_displacement == static_cast<int>(sentinel(28)));
	CHECK(info.hd_water_waves_normal_map_strength == static_cast<int>(sentinel(29)));
	CHECK(color(info.hd_water_override_color) == sentinel(30));
	CHECK(info.hd_water_envmap_reflectivity == static_cast<int>(sentinel(31)));
	CHECK(color(info.alpha_tile_minimap_color) == sentinel(32));
	REQUIRE(info.players.size() == 2);
	CHECK(info.players[0].internal_number == static_cast<int>(sentinel(33)));
	CHECK(info.players[0].hud_skin == sentinel(36));
	CHECK(info.players[0].fixed_start_position == static_cast<int>(sentinel(37)));

	// Force flags 0x3B: 0x08 is kept raw and does not turn into shared vision, which has its own ticket.
	REQUIRE(info.forces.size() == 1);
	CHECK(info.forces[0].unknown_flags == 0x08u);
	CHECK_FALSE(info.forces[0].share_vision);
	CHECK(info.forces[0].allied);
}

TEST_CASE("a padded player slot defaults hud_skin to 64") {
	const PlayerData player {};
	CHECK(player.hud_skin == 64u);
}

TEST_CASE("v39 stamps survive and v33 stamps stay what they were") {
	// The sentinel stamps must not be overwritten with the v33 values, and a v33 file must not keep its own.
	const std::string v39 = w3i_v39_bytes(7003);
	const fs::path dir = make_scratch_dir("w3i_stamps");
	const std::string saved_v39 = w3i_round_trip(dir, v39);
	CHECK(saved_v39.substr(0, 28) == v39.substr(0, 28));

	W3iSpec older = plain_v33_spec();
	older.version = 31;
	older.editor = 6059;
	const std::string saved_older = w3i_round_trip(dir, w3i_strict_bytes(older));
	CHECK(saved_older.substr(0, 4) == le32(33));
	CHECK(saved_older.substr(8, 20) == le32(6116) + le32(2) + le32(0) + le32(3) + le32(22978));
}

TEST_CASE("a v39 file with hud_skin on the wrong side of the editor gate does not read as correct") {
	// A wrong gate can still reach the end of the file with the names shifted, so this compares names.
	const fs::path dir = make_scratch_dir("w3i_v39_wrong_side");
	const auto reads_correctly = [&](const W3iSpec& spec) {
		write_all(dir / "war3map.w3i", w3i_strict_bytes(spec));
		const ScopedMapDirectory open_map(dir);
		try {
			MapInfo info;
			info.load();
			return info.players.size() == 2 && info.players[0].name == "TRIGSTR_014" && info.players[1].name == "TRIGSTR_015"
				&& info.forces.size() == 1 && info.forces[0].name == "TRIGSTR_016";
		} catch (const std::exception&) {
			return false;
		}
	};

	W3iSpec without_gate_bytes;
	without_gate_bytes.editor = 6127;
	CHECK(reads_correctly(without_gate_bytes));
	without_gate_bytes.hud_skin = true;
	CHECK_FALSE(reads_correctly(without_gate_bytes));

	W3iSpec with_gate_bytes;
	with_gate_bytes.editor = 6140;
	CHECK(reads_correctly(with_gate_bytes));
	with_gate_bytes.hud_skin = false;
	CHECK_FALSE(reads_correctly(with_gate_bytes));
}

TEST_CASE("v39 editors without samples are refused by the reader and the preflight") {
	for (const std::uint32_t editor : { 6128u, 6139u }) {
		CAPTURE(editor);
		const fs::path dir = make_valid_map("w3i_v39_unsampled");
		for (const bool hud_skin : { false, true }) {
			W3iSpec spec;
			spec.editor = editor;
			spec.hud_skin = hud_skin;
			write_all(dir / "war3map.w3i", w3i_strict_bytes(spec));

			check_refused_as_unsupported(dir, "war3map.w3i", "39");
			CHECK(contains(check_map_format_versions(dir).error(), std::to_string(editor)));

			const ScopedMapDirectory open_map(dir);
			MapInfo info;
			check_reader_refuses([&] { info.load(); }, "war3map.w3i", "39");
		}
	}
	const fs::path dir = make_valid_map("w3i_v39_sampled");
	for (const std::uint32_t editor : { 6127u, 6140u }) {
		CAPTURE(editor);
		write_all(dir / "war3map.w3i", w3i_v39_bytes(editor));
		CHECK(check_map_format_versions(dir).has_value());
	}
}

TEST_CASE("check_map_format_versions refuses a v39 war3map.w3i too short to hold its editor version") {
	const fs::path dir = make_valid_map("w3i_v39_short");
	const std::string header = le32(39) + le32(1) + le32(6140);
	for (std::size_t size = 4; size < header.size(); size++) {
		CAPTURE(size);
		write_all(dir / "war3map.w3i", header.substr(0, size));
		const auto result = check_map_format_versions(dir);
		REQUIRE_FALSE(result.has_value());
		CHECK(contains(result.error(), "war3map.w3i"));
		CHECK(contains(result.error(), "too short"));
	}
	write_all(dir / "war3map.w3i", header);
	CHECK(check_map_format_versions(dir).has_value());
}

TEST_CASE("a truncated v39 war3map.w3i throws at every length") {
	const fs::path dir = make_scratch_dir("w3i_v39_truncated");
	const ScopedMapDirectory open_map(dir);
	for (const std::uint32_t editor : { 6127u, 6140u }) {
		const std::string bytes = w3i_v39_bytes(editor);
		for (std::size_t size = 0; size < bytes.size(); size++) {
			CAPTURE(editor);
			CAPTURE(size);
			write_all(dir / "war3map.w3i", bytes.substr(0, size));
			MapInfo info;
			CHECK_THROWS(info.load());
		}
	}
}

TEST_CASE("a v39 war3map.w3i with trailing bytes throws") {
	const fs::path dir = make_scratch_dir("w3i_v39_trailing");
	const ScopedMapDirectory open_map(dir);
	for (const std::string trailing : { std::string(1, '\0'), std::string("x"), std::string(4, '\0') }) {
		write_all(dir / "war3map.w3i", w3i_v39_bytes(6140) + trailing);
		MapInfo info;
		CHECK_THROWS_AS(info.load(), std::runtime_error);
	}
}

TEST_CASE("v39 counts larger than the file are refused before anything is allocated") {
	const fs::path dir = make_scratch_dir("w3i_v39_counts");
	const ScopedMapDirectory open_map(dir);

	W3iSpec players;
	players.player_count = 0xFFFF4700;
	write_all(dir / "war3map.w3i", w3i_strict_bytes(players));
	MapInfo player_info;
	CHECK_THROWS_AS(player_info.load(), std::runtime_error);

	W3iSpec forces;
	forces.force_count = 0xFFFF4700;
	write_all(dir / "war3map.w3i", w3i_strict_bytes(forces));
	MapInfo force_info;
	CHECK_THROWS_AS(force_info.load(), std::runtime_error);

	// The file behind the shipped crash, had it really been v39: nothing in it parses as a v39 layout.
	write_all(dir / "war3map.w3i", w3i_crash_bytes(39));
	MapInfo crash_info;
	CHECK_THROWS_AS(crash_info.load(), std::runtime_error);
}

TEST_CASE("a v33 war3map.w3i without the optional sections still loads") {
	const fs::path dir = make_scratch_dir("w3i_v33_short");
	W3iSpec spec = plain_v33_spec();
	spec.stop_after_forces = true;
	write_all(dir / "war3map.w3i", w3i_strict_bytes(spec));
	const ScopedMapDirectory open_map(dir);
	MapInfo info;
	CHECK_NOTHROW(info.load());
	CHECK(info.available_upgrades.empty());
}

TEST_CASE("a plain v33 war3map.w3i round-trips byte-exact") {
	const std::string bytes = w3i_strict_bytes(plain_v33_spec());
	check_same_bytes(w3i_round_trip(make_scratch_dir("w3i_v33_plain"), bytes), bytes);
}

TEST_CASE("MapInfo::save keeps map flag bits above 0x400000") {
	W3iSpec spec = plain_v33_spec();
	spec.flags = 0x2180ABCD;
	const std::string bytes = w3i_strict_bytes(spec);
	check_same_bytes(w3i_round_trip(make_scratch_dir("w3i_flags"), bytes), bytes);
}

TEST_CASE("MapInfo::save keeps force flag bit 0x08") {
	W3iSpec spec = plain_v33_spec();
	spec.force_flags = 0x3B;
	const std::string bytes = w3i_strict_bytes(spec);
	check_same_bytes(w3i_round_trip(make_scratch_dir("w3i_force_flags"), bytes), bytes);
}

TEST_CASE("MapInfo::save writes the random unit table position count") {
	W3iSpec spec = plain_v33_spec();
	spec.random_tables = true;
	const std::string bytes = w3i_strict_bytes(spec);
	check_same_bytes(w3i_round_trip(make_scratch_dir("w3i_positions"), bytes), bytes);
}

TEST_CASE("MapInfo::save keeps four bytes for a random table ID of NUL bytes") {
	W3iSpec spec = plain_v33_spec();
	spec.random_tables = true;
	spec.null_table_ids = true;
	const std::string bytes = w3i_strict_bytes(spec);
	check_same_bytes(w3i_round_trip(make_scratch_dir("w3i_table_ids"), bytes), bytes);
}

TEST_CASE("MapInfo::save keeps four bytes for an upgrade or tech ID of NUL bytes") {
	W3iSpec spec = plain_v33_spec();
	spec.null_upgrade_ids = true;
	const std::string bytes = w3i_strict_bytes(spec);
	check_same_bytes(w3i_round_trip(make_scratch_dir("w3i_upgrade_ids"), bytes), bytes);
}

TEST_CASE("force flags 0x3B and a NUL random table cell survive a v39 save") {
	const std::string bytes = w3i_v39_bytes(6140);
	CHECK(contains(bytes, le32(0x3B)));
	CHECK(contains(bytes, std::string(4, '\0')));
	check_same_bytes(w3i_round_trip(make_scratch_dir("w3i_v39_preserved"), bytes), bytes);
}

TEST_CASE("a copy of the test map war3map.w3i round-trips byte-exact") {
	const std::string bytes = read_all(fs::path(TEST_MAP_DIR) / "war3map.w3i");
	REQUIRE(w3i_version_of(bytes) == 33);
	check_same_bytes(w3i_round_trip(make_scratch_dir("w3i_test_map"), bytes, w3i_tileset_byte(bytes)), bytes);
}

TEST_CASE("older war3map.w3i versions saved through a stack MapInfo write zeros for fields they lack") {
	for (const std::uint32_t version : { 25u, 28u, 31u }) {
		CAPTURE(version);
		W3iSpec input = plain_v33_spec();
		input.version = version;
		input.editor = 6059;
		W3iSpec expected = plain_v33_spec();
		expected.saved_from = version;

		// On dirty stack bytes, so this cannot pass by luck: an unset field shows up as 0xCD in the saved file.
		const fs::path dir = make_scratch_dir("w3i_older_saved");
		write_all(dir / "war3map.w3i", w3i_strict_bytes(input));
		const ScopedMapDirectory open_map(dir);
		alignas(MapInfo) unsigned char storage[sizeof(MapInfo)];
		std::memset(storage, 0xCD, sizeof(storage));
		MapInfo* const info = new (storage) MapInfo;
		info->load();
		info->save('L');
		info->~MapInfo();
		check_same_bytes(read_all(dir / "war3map.w3i"), w3i_strict_bytes(expected));
	}
}

TEST_CASE("v15 and v18 leave the fog, weather, water and lua fields they lack at zero") {
	const fs::path dir = make_scratch_dir("w3i_v15_v18_defaults");
	const ScopedMapDirectory open_map(dir);
	const auto color = [](const auto value) { return std::bit_cast<std::uint32_t>(value); };
	for (const std::uint32_t version : { 15u, 18u }) {
		CAPTURE(version);
		write_all(dir / "war3map.w3i", w3i_bytes(version));
		alignas(MapInfo) unsigned char storage[sizeof(MapInfo)];
		std::memset(storage, 0xCD, sizeof(storage));
		MapInfo* const info = new (storage) MapInfo;
		info->load();
		CHECK(info->fog_style == 0);
		CHECK(info->fog_start_z_height == 0.f);
		CHECK(info->fog_end_z_height == 0.f);
		CHECK(info->fog_density == 0.f);
		CHECK(color(info->fog_color) == 0u);
		CHECK(info->weather_id == 0);
		CHECK(color(info->water_color) == 0u);
		// Compared as a byte: 0xCD in a bool is not a value the compiler has to read back as true.
		CHECK(std::bit_cast<unsigned char>(info->lua) == 0);
		CHECK(info->supported_modes == 0u);
		CHECK(info->game_data_version == 0u);
		info->~MapInfo();
	}
}

TEST_CASE("every stock v39 war3map.w3i below HIVEWE_STOCK_W3I_ROOT round-trips byte-exact") {
#pragma warning(suppress : 4996) // getenv: the variable is read once on a single-threaded test.
	const char* const root = std::getenv("HIVEWE_STOCK_W3I_ROOT");
	if (root == nullptr || *root == '\0') {
		MESSAGE("skipped: HIVEWE_STOCK_W3I_ROOT is not set");
		return;
	}

	const fs::path scratch = make_scratch_dir("w3i_stock_sweep");
	int checked = 0;
	for (const auto& entry : fs::recursive_directory_iterator(root)) {
		if (!entry.is_regular_file() || entry.path().filename() != "war3map.w3i") {
			continue;
		}
		const std::string bytes = read_all(entry.path());
		if (w3i_version_of(bytes) != 39) {
			continue;
		}
		checked++;
		CAPTURE(entry.path().string());
		// Accepting w3i v39 opens no stock map yet: w3r and w3c still refuse them. Drop this when they stop.
		CHECK_FALSE(check_map_format_versions(entry.path().parent_path()).has_value());
		try {
			check_same_bytes(w3i_round_trip(scratch, bytes, w3i_tileset_byte(bytes)), bytes);
		} catch (const std::exception& e) {
			FAIL_CHECK("load threw: ", std::string(e.what()));
		}
	}
	MESSAGE("stock v39 files checked: ", checked);
	CHECK(checked > 0);
}
