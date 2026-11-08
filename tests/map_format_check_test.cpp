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

	/// A v39 w3i as HiveWE used to misread it: 163 is where MapInfo::load's v33 layout reads the player
	/// count, and the stock 3.0 map has the unrelated bytes 0xFFFF4700 there. That is the shipped crash.
	std::string w3i_v39_crash_bytes() {
		std::string bytes = w3i_bytes(39);
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
	for (const std::uint32_t version : { 15u, 18u, 25u, 26u, 28u, 31u, 32u, 33u }) {
		CAPTURE(version);
		write_all(dir / "war3map.w3i", w3i_bytes(version));
		CHECK(check_map_format_versions(dir).has_value());
	}
	for (const std::uint32_t version : { 0u, 8u, 10u, 11u, 23u, 24u, 27u }) {
		CAPTURE(version);
		write_all(dir / "war3map.w3i", w3i_bytes(version));
		check_refused_as_unsupported(dir, "war3map.w3i", std::to_string(version));
	}
	for (const std::uint32_t version : { 34u, 39u, 0xFFFFFFFFu }) {
		CAPTURE(version);
		write_all(dir / "war3map.w3i", w3i_bytes(version));
		check_refused_as_newer(dir, "war3map.w3i", std::to_string(version));
	}
}

TEST_CASE("check_map_format_versions accepts and refuses war3map.doo versions, ignoring the subversion") {
	const fs::path dir = make_valid_map("doo");
	for (const std::uint32_t version : { 7u, 8u }) {
		for (const std::uint32_t subversion : { 0u, 9u, 11u, 99u }) {
			CAPTURE(version);
			CAPTURE(subversion);
			write_all(dir / "war3map.doo", doo_bytes(version, subversion));
			CHECK(check_map_format_versions(dir).has_value());
		}
	}
	write_all(dir / "war3map.doo", doo_bytes(6));
	check_refused_as_unsupported(dir, "war3map.doo", "6");
	for (const std::uint32_t version : { 9u, 12u, 13u }) {
		CAPTURE(version);
		write_all(dir / "war3map.doo", doo_bytes(version));
		check_refused_as_newer(dir, "war3map.doo", std::to_string(version));
	}
}

TEST_CASE("check_map_format_versions accepts and refuses war3mapUnits.doo versions and subversions") {
	const fs::path dir = make_valid_map("units");
	for (const std::uint32_t version : { 7u, 8u }) {
		for (const std::uint32_t subversion : { 9u, 11u }) {
			CAPTURE(version);
			CAPTURE(subversion);
			write_all(dir / "war3mapUnits.doo", doo_bytes(version, subversion));
			CHECK(check_map_format_versions(dir).has_value());
		}
	}
	write_all(dir / "war3mapUnits.doo", doo_bytes(6, 9));
	check_refused_as_unsupported(dir, "war3mapUnits.doo", "6");
	for (const std::uint32_t version : { 12u, 13u }) {
		CAPTURE(version);
		write_all(dir / "war3mapUnits.doo", doo_bytes(version, 9));
		check_refused_as_newer(dir, "war3mapUnits.doo", std::to_string(version));
	}
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
	write_all(dir / "war3mapUnits.doo", "XXXX" + le32(13) + le32(11));
	check_refused_as_newer(dir, "war3mapUnits.doo", "13");
}

TEST_CASE("check_map_format_versions reports every failing file") {
	const fs::path dir = make_valid_map("all_failures");
	write_all(dir / "war3map.w3i", w3i_bytes(39));
	write_all(dir / "war3map.doo", doo_bytes(13));
	write_all(dir / "war3mapUnits.doo", doo_bytes(13));
	write_all(dir / "war3map.w3r", count_file_bytes(7));
	write_all(dir / "war3map.w3c", count_file_bytes(3));
	write_all(dir / "war3map.w3s", "");
	const auto result = check_map_format_versions(dir);
	REQUIRE_FALSE(result.has_value());
	for (const std::string part : { "war3map.w3i", "version 39", "war3map.doo", "war3mapUnits.doo", "version 13", "war3map.w3r", "version 7",
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
	write_all(dir / "war3map.w3i", w3i_bytes(39));
	write_all(dir / "war3map.doo", doo_bytes(6));
	const auto result = check_map_format_versions(dir);
	REQUIRE_FALSE(result.has_value());
	CHECK(contains(result.error(), "newer version of HiveWE"));
	CHECK(contains(result.error(), "war3map.doo is version 6, which HiveWE does not support"));
}

TEST_CASE("require_accepted words the exception like the preflight") {
	CHECK_THROWS_WITH_AS(
		map_format::require_accepted("war3map.w3i", map_format::w3i_versions, 39),
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
	write_all(dir / "war3map.w3i", w3i_bytes(39));
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
		{ "war3map.w3i", w3i_bytes(39) },
		{ "war3map.w3e", w3e_bytes(13) },
		{ "war3map.doo", doo_bytes(13) },
		{ "war3mapUnits.doo", doo_bytes(13) },
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

TEST_CASE("MapInfo::load refuses war3map.w3i v39 instead of allocating from misread bytes") {
	const fs::path dir = make_scratch_dir("reader_w3i_v39");
	write_all(dir / "war3map.w3i", w3i_v39_crash_bytes());
	const ScopedMapDirectory open_map(dir);
	MapInfo info;
	check_reader_refuses([&] { info.load(); }, "war3map.w3i", "39");
}

TEST_CASE("MapInfo::load still reads every accepted version") {
	const fs::path dir = make_scratch_dir("reader_w3i_accepted");
	const ScopedMapDirectory open_map(dir);
	for (const std::uint32_t version : map_format::w3i_versions) {
		CAPTURE(version);
		write_all(dir / "war3map.w3i", w3i_bytes(version));
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
	write_all(dir / "war3map.doo", doo_bytes(13));
	Doodads doodads;
	MapInfo info;
	check_reader_refuses([&] { doodads.load(terrain_without_gl(), info); }, "war3map.doo", "13");
}

TEST_CASE("Units::load refuses an unknown war3mapUnits.doo version or subversion") {
	const fs::path dir = make_scratch_dir("reader_units");
	const ScopedMapDirectory open_map(dir);
	Units units;
	MapInfo info;
	write_all(dir / "war3mapUnits.doo", doo_bytes(13, 11));
	check_reader_refuses([&] { units.load(terrain_without_gl(), info); }, "war3mapUnits.doo", "13");
	write_all(dir / "war3mapUnits.doo", doo_bytes(8, 10));
	check_reader_refuses([&] { units.load(terrain_without_gl(), info); }, "war3mapUnits.doo", "10");
}

TEST_CASE("MapProtector reports a v39 war3map.w3i as a failure and does not rewrite it") {
	const fs::path dir = make_scratch_dir("protector_v39");
	write_all(dir / "war3map.w3i", w3i_v39_crash_bytes());
	write_all(dir / "war3map.w3e", w3e_bytes(12));
	const Snapshot before = snapshot(dir);

	RenameCandidate candidate;
	candidate.original_relative_path = "war3mapImported/Custom.mdx";
	candidate.new_relative_path = "a3f9c1e2.mdx";
	candidate.match_key = asset_match_key("war3mapImported/Custom.mdx");

	const ScopedMapDirectory open_map("some/open/map");
	const AssetObfuscationResult result = rewrite_map_info_references(dir, { candidate });
	CHECK_FALSE(result.success);
	CHECK(contains(result.error, "war3map.w3i"));
	CHECK(contains(result.error, "version 39"));
	CHECK(hierarchy.map_directory == fs::path("some/open/map"));
	CHECK(snapshot(dir) == before);
}
