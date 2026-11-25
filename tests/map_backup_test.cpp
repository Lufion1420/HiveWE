#define DOCTEST_CONFIG_USE_STD_HEADERS
#include <doctest/doctest.h>

import std;
import MapBackup;
import Hierarchy;
import Imports;
import ScriptPreservation;

namespace fs = std::filesystem;
using namespace std::chrono_literals;

// Versioned backups before editor saves (Asana 1220329518098116).

namespace {
	fs::path make_scratch_dir(const std::string& name) {
		const fs::path dir = fs::temp_directory_path() / "hivewe_map_backup_test" / name;
		std::error_code ec;
		if (fs::exists(dir, ec)) {
			for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
				fs::permissions(entry.path(), fs::perms::owner_write, fs::perm_options::add, ec);
			}
		}
		fs::remove_all(dir, ec);
		fs::create_directories(dir, ec);
		return dir;
	}

	std::string read_all(const fs::path& path) {
		std::ifstream in(path, std::ios::binary);
		return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
	}

	void write_all(const fs::path& path, const std::string& content) {
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		out << content;
	}

	void make_read_only(const fs::path& path) {
		fs::permissions(path, fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write, fs::perm_options::remove);
	}

	bool is_read_only(const fs::path& path) {
		return (fs::status(path).permissions() & fs::perms::owner_write) == fs::perms::none;
	}

	/// Every path under `folder` with its bytes, to show a folder was not changed.
	std::map<fs::path, std::string> snapshot(const fs::path& folder) {
		std::map<fs::path, std::string> files;
		for (const auto& entry : fs::recursive_directory_iterator(folder)) {
			files[entry.path().lexically_relative(folder)] = entry.is_regular_file() ? read_all(entry.path()) : "<dir>";
		}
		return files;
	}

	const map_backup::LocalTime base_time = std::chrono::local_days(2026y / std::chrono::November / 1) + 12h;

	map_backup::Options options_at(const fs::path& root, const map_backup::LocalTime now, const std::string& session = "s1", const size_t keep = 10) {
		return { .root = root, .now = now, .session_id = session, .keep = keep };
	}

	/// A small map folder with the files a save writes.
	void make_map(const fs::path& folder, const std::string& content = "A") {
		fs::create_directories(folder);
		write_all(folder / "war3map.w3e", "TERRAIN " + content);
		write_all(folder / "war3map.w3i", "INFO " + content);
		write_all(folder / "war3mapUnits.doo", "UNITS " + content);
	}

	std::set<std::wstring> set_names(const fs::path& map_backups) {
		std::set<std::wstring> names;
		for (const auto& set : map_backup::list_sets(map_backups)) {
			names.insert(set.path.filename().native());
		}
		return names;
	}

	/// One backup at `now` after changing the map's terrain, so it is never identical to the previous one.
	map_backup::Result back_up_changed(const fs::path& map, const map_backup::Options& options) {
		static int counter = 0;
		write_all(map / "war3map.w3e", "TERRAIN " + std::to_string(++counter));
		const auto result = map_backup::back_up(map, options);
		REQUIRE_MESSAGE(result.has_value(), result.error().message);
		REQUIRE(result->outcome == map_backup::Outcome::created);
		return *result;
	}
} // namespace

TEST_CASE("map backup selects the map's own root files, scripts and conversation.json only") {
	const fs::path map = make_scratch_dir("selection");
	make_map(map);
	write_all(map / "War3MapMisc.txt", "MISC");
	write_all(map / "conversation.json", "{}");
	write_all(map / "notes.txt", "user notes");
	write_all(map / "imported.mdx", "MDX");
	write_all(map / "war3map.w3e.hivewe-tmp", "LEFT BY A CRASH");
	fs::create_directories(map / "war3mapImported");
	write_all(map / "war3mapImported" / "war3map.w3e", "AN IMPORT");
	fs::create_directories(map / "scripts");
	write_all(map / "scripts" / "war3map.j", "JASS");
	write_all(map / "scripts" / "War3Map.lua", "LUA");
	write_all(map / "scripts" / "other.j", "OTHER");
	fs::create_directories(map / "sub");
	write_all(map / "sub" / "war3map.j", "NOT A MAP SCRIPT");

	const auto files = map_backup::select_files(map);
	const std::vector<fs::path> expected = {
		"War3MapMisc.txt",
		"conversation.json",
		fs::path("scripts") / "War3Map.lua",
		fs::path("scripts") / "war3map.j",
		"war3map.w3e",
		"war3map.w3i",
		"war3mapUnits.doo",
	};
	std::vector<fs::path> sorted_expected = expected;
	std::ranges::sort(sorted_expected);
	CHECK(files == sorted_expected);
}

TEST_CASE("map backup selection covers every file Save writes and every war3map name of the imports blacklist") {
	// Every file Map::save and Imports::save write, and the script files Triggers writes or keeps.
	const std::vector<std::string> save_writes = {
		"war3map.wpm", "war3map.w3e", "war3map.shd", "war3map.w3d", "war3mapSkin.w3d", "war3map.w3b", "war3mapSkin.w3b", "war3map.doo",
		"war3map.w3u", "war3mapSkin.w3u", "war3map.w3t", "war3mapSkin.w3t", "war3mapUnits.doo", "war3map.w3a", "war3mapSkin.w3a",
		"war3map.w3h", "war3mapSkin.w3h", "war3map.w3q", "war3mapSkin.w3q", "war3map.w3r", "war3map.w3i", "war3map.wts", "war3map.wtg",
		"war3map.wct", "war3map.j", "war3map.lua", "war3mapMisc.txt", "war3map.imp",
	};
	for (const auto& name : save_writes) {
		CHECK_MESSAGE(map_backup::is_backed_up_name(name), name);
	}
	for (const auto& name : Imports().blacklist) {
		if (name.starts_with("war3map") || name == "conversation.json") {
			CHECK_MESSAGE(map_backup::is_backed_up_name(name), name);
		}
	}
	for (const auto name : map_script_file_names) {
		CHECK_MESSAGE(map_backup::is_backed_up_name(fs::path(name)), name);
	}
	CHECK_FALSE(map_backup::is_backed_up_name(std::string("war3map.w3e") + std::string(map_write_temp_suffix)));
	CHECK_FALSE(map_backup::is_backed_up_name(fs::path("war3mapImported") / "war3map.w3e"));
	CHECK_FALSE(map_backup::is_backed_up_name("imported.blp"));
}

TEST_CASE("map backup copies the files before a save and skips a backup identical to the newest one") {
	const fs::path dir = make_scratch_dir("identical");
	const fs::path map = dir / "map";
	make_map(map);
	const auto before = snapshot(map);

	const auto first = map_backup::back_up(map, options_at(dir / "backups", base_time));
	REQUIRE_MESSAGE(first.has_value(), first.error().message);
	CHECK(first->outcome == map_backup::Outcome::created);
	CHECK(first->set == first->map_backups / map_backup::set_name(base_time));
	CHECK(read_all(first->set / "war3map.w3e") == "TERRAIN A");
	CHECK(read_all(first->set / "war3mapUnits.doo") == "UNITS A");
	CHECK(snapshot(map) == before);

	const auto second = map_backup::back_up(map, options_at(dir / "backups", base_time + 1min));
	REQUIRE(second.has_value());
	CHECK(second->outcome == map_backup::Outcome::identical);
	CHECK(second->set == first->set);
	CHECK(map_backup::list_sets(first->map_backups).size() == 1);

	write_all(map / "war3map.w3e", "TERRAIN B");
	const auto third = map_backup::back_up(map, options_at(dir / "backups", base_time + 2min));
	REQUIRE(third.has_value());
	CHECK(third->outcome == map_backup::Outcome::created);
	CHECK(map_backup::list_sets(first->map_backups).size() == 2);

	// A file added to the map also makes it differ.
	write_all(map / "war3mapMisc.txt", "MISC");
	const auto fourth = map_backup::back_up(map, options_at(dir / "backups", base_time + 3min));
	REQUIRE(fourth.has_value());
	CHECK(fourth->outcome == map_backup::Outcome::created);
}

TEST_CASE("map backup gives two backups in the same millisecond separate names") {
	const fs::path dir = make_scratch_dir("same_time");
	const fs::path map = dir / "map";
	make_map(map);
	const auto first = map_backup::back_up(map, options_at(dir / "backups", base_time));
	write_all(map / "war3map.w3e", "CHANGED");
	const auto second = map_backup::back_up(map, options_at(dir / "backups", base_time));
	REQUIRE(first.has_value());
	REQUIRE(second.has_value());
	CHECK(second->set.filename() == map_backup::set_name(base_time, 1));
	CHECK(map_backup::list_sets(first->map_backups).front().path == second->set);
}

TEST_CASE("map backup retention keeps the newest ones and, for 14 days, the first of each session and each day") {
	const fs::path dir = make_scratch_dir("retention");
	const fs::path map = dir / "map";
	make_map(map);
	const fs::path root = dir / "backups";

	// Day 1: three saves in session s1, three in s2; keep 2.
	std::vector<map_backup::Result> day_one;
	for (int i = 0; i < 6; i++) {
		day_one.push_back(back_up_changed(map, options_at(root, base_time + std::chrono::minutes(i), i < 3 ? "s1" : "s2", 2)));
	}
	const fs::path map_backups = day_one.front().map_backups;
	// Newest two (4, 5), first of the day and of s1 (0), first of s2 (3).
	CHECK(set_names(map_backups)
		  == std::set<std::wstring> { map_backup::set_name(base_time), map_backup::set_name(base_time + 3min), map_backup::set_name(base_time + 4min),
									  map_backup::set_name(base_time + 5min) });

	// The next day, same session s2: its first of the day is pinned too.
	const auto next_day = base_time + std::chrono::days(1);
	back_up_changed(map, options_at(root, next_day, "s2", 2));
	back_up_changed(map, options_at(root, next_day + 1min, "s2", 2));
	back_up_changed(map, options_at(root, next_day + 2min, "s2", 2));
	CHECK(set_names(map_backups)
		  == std::set<std::wstring> { map_backup::set_name(base_time), map_backup::set_name(base_time + 3min), map_backup::set_name(next_day),
									  map_backup::set_name(next_day + 1min), map_backup::set_name(next_day + 2min) });

	// 20 days later the pins have expired; only the newest two remain.
	const auto later = base_time + std::chrono::days(20);
	const auto last = back_up_changed(map, options_at(root, later, "s3", 2));
	CHECK(set_names(map_backups) == std::set<std::wstring> { map_backup::set_name(next_day + 2min), map_backup::set_name(later) });
	CHECK(last.removed.size() == 4);
}

TEST_CASE("map backup removes a stale .partial set, ignores it for the identical check and never touches foreign folders") {
	const fs::path dir = make_scratch_dir("partial");
	const fs::path map = dir / "map";
	make_map(map);
	const fs::path root = dir / "backups";
	const fs::path map_backups = map_backup::backups_folder_for(root, map);
	fs::create_directories(map_backups);

	// A partial set with the same content as the map, as a crash would leave it, one file read-only.
	const fs::path partial = map_backups / (map_backup::set_name(base_time - 1h) + L".partial");
	fs::create_directories(partial);
	for (const auto& file : map_backup::select_files(map)) {
		fs::copy_file(map / file, partial / file);
	}
	write_all(partial / "source.txt", "x\n");
	make_read_only(partial / "war3map.w3e");
	// Things rotation must never delete.
	fs::create_directories(map_backups / "keep me");
	write_all(map_backups / "keep me" / "war3map.w3e", "USER");
	write_all(map_backups / "notes.txt", "USER");
	const fs::path no_source = map_backups / map_backup::set_name(base_time - 2h);
	fs::create_directories(no_source);
	write_all(no_source / "war3map.w3e", "NOT OURS: NO source.txt");
	fs::create_directories(dir / "elsewhere");
	write_all(dir / "elsewhere" / "war3map.w3e", "USER");

	const auto result = map_backup::back_up(map, options_at(root, base_time, "s1", 1));
	REQUIRE_MESSAGE(result.has_value(), result.error().message);
	CHECK(result->outcome == map_backup::Outcome::created);
	CHECK_FALSE(fs::exists(partial));

	// 30 days on, then 50 days on: no pin is left on the older set, so keep = 1 leaves one set.
	back_up_changed(map, options_at(root, base_time + std::chrono::days(30), "s2", 1));
	back_up_changed(map, options_at(root, base_time + std::chrono::days(50), "s3", 1));
	CHECK(map_backup::list_sets(map_backups).size() == 1);
	CHECK(read_all(map_backups / "keep me" / "war3map.w3e") == "USER");
	CHECK(read_all(map_backups / "notes.txt") == "USER");
	CHECK(read_all(no_source / "war3map.w3e") == "NOT OURS: NO source.txt");
	CHECK(read_all(dir / "elsewhere" / "war3map.w3e") == "USER");
}

TEST_CASE("map backup never removes the set it just made, even when the clock went back") {
	const fs::path dir = make_scratch_dir("clock_back");
	const fs::path map = dir / "map";
	make_map(map);
	const fs::path root = dir / "backups";
	// Same session and day, keep 2: base is pinned (first of session and day), +3h and +4h are the newest two.
	for (const auto offset : { 0h, 2h, 3h, 4h }) {
		back_up_changed(map, options_at(root, base_time + offset, "s1", 2));
	}
	// The clock went back an hour or more: the new set sorts below the newest two and is neither first of session nor day.
	const auto after_clock_change = back_up_changed(map, options_at(root, base_time + 1h, "s1", 2));
	CHECK(fs::exists(after_clock_change.set));
	CHECK(std::ranges::find(after_clock_change.removed, after_clock_change.set) == after_clock_change.removed.end());
	CHECK(read_all(after_clock_change.set / "war3map.w3e") == read_all(map / "war3map.w3e"));
}

TEST_CASE("map backup leaves a set alone when it cannot be moved aside for removal, and does not report it removed") {
	const fs::path dir = make_scratch_dir("locked_set");
	const fs::path map = dir / "map";
	make_map(map);
	const fs::path root = dir / "backups";
	const auto old_set = back_up_changed(map, options_at(root, base_time, "s1", 1));
	const auto before = snapshot(old_set.set);
	std::vector<fs::path> removed;
	{
		// An open file inside (a virus scanner reading it) stops the folder from being renamed.
		std::ifstream held(old_set.set / "war3map.w3e", std::ios::binary);
		REQUIRE(held.is_open());
		removed = back_up_changed(map, options_at(root, base_time + std::chrono::days(30), "s2", 1)).removed;
	}
	CHECK(std::ranges::find(removed, old_set.set) == removed.end());
	CHECK(snapshot(old_set.set) == before);
	CHECK(map_backup::list_sets(old_set.map_backups).size() == 2);

	// Unlocked, the next run removes it.
	const auto later = back_up_changed(map, options_at(root, base_time + std::chrono::days(31), "s3", 1));
	CHECK_FALSE(fs::exists(old_set.set));
	CHECK(std::ranges::find(later.removed, old_set.set) != later.removed.end());
}

TEST_CASE("map backup sweeps a half-removed .deleting set and never counts it as a backup") {
	const fs::path dir = make_scratch_dir("deleting");
	const fs::path map = dir / "map";
	make_map(map);
	const fs::path root = dir / "backups";
	const fs::path map_backups = map_backup::backups_folder_for(root, map);
	// A removal that stopped partway: same content as the map, source.txt still there, one file read-only.
	const fs::path deleting = map_backups / (map_backup::set_name(base_time - 1h) + L".deleting");
	fs::create_directories(deleting);
	for (const auto& file : map_backup::select_files(map)) {
		fs::copy_file(map / file, deleting / file);
	}
	write_all(deleting / "source.txt", "x\n");
	make_read_only(deleting / "war3map.w3e");
	fs::create_directories(map_backups / "keep me.deleting");

	CHECK(map_backup::list_sets(map_backups).empty());
	const auto result = map_backup::back_up(map, options_at(root, base_time, "s1"));
	REQUIRE_MESSAGE(result.has_value(), result.error().message);
	CHECK(result->outcome == map_backup::Outcome::created);
	CHECK_FALSE(fs::exists(deleting));
	CHECK(fs::exists(map_backups / "keep me.deleting"));
}

TEST_CASE("map backup handles Cyrillic and CJK folder and file names") {
	const fs::path dir = make_scratch_dir("unicode");
	const fs::path map = dir / L"Карта 地图 with a name that is longer than thirty-two characters";
	make_map(map);
	write_all(map / L"war3map_Ж字.txt", "UNICODE");

	const auto result = map_backup::back_up(map, options_at(dir / "backups", base_time));
	REQUIRE_MESSAGE(result.has_value(), result.error().message);
	CHECK(result->outcome == map_backup::Outcome::created);
	CHECK(read_all(result->set / L"war3map_Ж字.txt") == "UNICODE");

	const std::wstring folder = result->map_backups.filename().native();
	CHECK(folder.starts_with(std::wstring(L"Карта 地图 with a name that is longer than thirty-two characters").substr(0, 32) + L"-"));
	CHECK(folder.size() == 32 + 1 + 16);

	const std::u8string expected_path = map_backup::normalized_folder(map).u8string();
	CHECK(read_all(result->set / "source.txt") == std::string(expected_path.begin(), expected_path.end()) + "\n");
}

TEST_CASE("map backup copies a read-only map file and rotation later removes the copy") {
	const fs::path dir = make_scratch_dir("read_only");
	const fs::path map = dir / "map";
	make_map(map);
	write_all(map / "war3map.w3i", "READ ONLY INFO");
	make_read_only(map / "war3map.w3i");
	const fs::path root = dir / "backups";

	const auto first = map_backup::back_up(map, options_at(root, base_time, "s1", 1));
	REQUIRE_MESSAGE(first.has_value(), first.error().message);
	CHECK(read_all(first->set / "war3map.w3i") == "READ ONLY INFO");
	// Make the copy read-only too, as an older backup or another tool may have left it.
	make_read_only(first->set / "war3map.w3i");

	back_up_changed(map, options_at(root, base_time + std::chrono::days(30), "s2", 1));
	CHECK_FALSE(fs::exists(first->set));
	CHECK(map_backup::list_sets(first->map_backups).size() == 1);
	CHECK(is_read_only(map / "war3map.w3i"));
	CHECK(read_all(map / "war3map.w3i") == "READ ONLY INFO");
	fs::permissions(map / "war3map.w3i", fs::perms::owner_write, fs::perm_options::add);
}

TEST_CASE("map backup keeps two folders with the same name apart") {
	const fs::path dir = make_scratch_dir("same_name");
	const fs::path first_map = dir / "a" / "NCOW";
	const fs::path second_map = dir / "b" / "NCOW";
	make_map(first_map, "FIRST");
	make_map(second_map, "SECOND");
	const fs::path root = dir / "backups";

	const auto first = map_backup::back_up(first_map, options_at(root, base_time, "s1", 1));
	const auto second = map_backup::back_up(second_map, options_at(root, base_time, "s1", 1));
	REQUIRE(first.has_value());
	REQUIRE(second.has_value());
	CHECK(first->map_backups != second->map_backups);
	CHECK(first->map_backups.filename().native().starts_with(L"NCOW-"));
	CHECK(second->map_backups.filename().native().starts_with(L"NCOW-"));
	CHECK(read_all(first->set / "war3map.w3e") == "TERRAIN FIRST");
	CHECK(read_all(second->set / "war3map.w3e") == "TERRAIN SECOND");

	// Rotating one never touches the other.
	back_up_changed(first_map, options_at(root, base_time + std::chrono::days(30), "s2", 1));
	CHECK(fs::exists(second->set));
	CHECK_FALSE(fs::exists(first->set));
}

TEST_CASE("map backup folder ignores letter case and a trailing separator, and hashes with FNV-1a 64") {
	CHECK(map_backup::fnv1a_64_hex("") == "cbf29ce484222325");
	CHECK(map_backup::fnv1a_64_hex("a") == "af63dc4c8601ec8c");
	const fs::path dir = make_scratch_dir("folder_key");
	fs::create_directories(dir / "Map");
	const fs::path root = dir / "backups";
	CHECK(map_backup::backups_folder_for(root, dir / "Map") == map_backup::backups_folder_for(root, dir / "Map" / ""));
	CHECK(map_backup::backups_folder_for(root, dir / "Map").filename().native().substr(4)
		  == map_backup::backups_folder_for(root, dir / "MAP").filename().native().substr(4));
}

TEST_CASE("map backup set names round-trip and other names are not sets") {
	const auto parsed = map_backup::parse_set_name(map_backup::set_name(base_time + 1234ms, 3));
	REQUIRE(parsed.has_value());
	CHECK(parsed->first == base_time + 1234ms);
	CHECK(parsed->second == 3);
	CHECK(map_backup::set_name(base_time + 1234ms) == L"20261101-120001-234");
	CHECK_FALSE(map_backup::parse_set_name(L"20261301-120000-000"));
	CHECK_FALSE(map_backup::parse_set_name(L"20261101-120000-000.partial"));
	CHECK_FALSE(map_backup::parse_set_name(L"20261101-120000-000-0"));
	CHECK_FALSE(map_backup::parse_set_name(L"keep me"));
}

TEST_CASE("a failed map backup reports the folder and leaves the map untouched") {
	const fs::path dir = make_scratch_dir("root_is_file");
	const fs::path map = dir / "map";
	make_map(map);
	write_all(dir / "backups", "A FILE WHERE THE BACKUP ROOT SHOULD BE");
	const auto before = snapshot(map);

	std::expected<map_backup::Result, map_backup::Error> result;
	CHECK_NOTHROW(result = map_backup::back_up(map, options_at(dir / "backups", base_time)));
	REQUIRE_FALSE(result.has_value());
	CHECK_FALSE(result.error().message.empty());
	CHECK_FALSE(result.error().folder.empty());
	CHECK(snapshot(map) == before);
	CHECK(read_all(dir / "backups") == "A FILE WHERE THE BACKUP ROOT SHOULD BE");
}

TEST_CASE("map backup of an empty or missing folder has nothing to do") {
	const fs::path dir = make_scratch_dir("nothing");
	fs::create_directories(dir / "empty");
	write_all(dir / "empty" / "notes.txt", "not a map file");

	const auto empty = map_backup::back_up(dir / "empty", options_at(dir / "backups", base_time));
	REQUIRE(empty.has_value());
	CHECK(empty->outcome == map_backup::Outcome::nothing_to_back_up);
	const auto missing = map_backup::back_up(dir / "missing", options_at(dir / "backups", base_time));
	REQUIRE(missing.has_value());
	CHECK(missing->outcome == map_backup::Outcome::nothing_to_back_up);
	CHECK_FALSE(fs::exists(dir / "backups"));
}
