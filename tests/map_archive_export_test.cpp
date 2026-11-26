#define DOCTEST_CONFIG_USE_STD_HEADERS
#include <doctest/doctest.h>
#define __STORMLIB_NO_STATIC_LINK__
#include "StormLib.h"

import std;
import Hierarchy;
import MapArchiveExport;

namespace fs = std::filesystem;
using map_archive::Step;

// Export MPQ's archive step (Asana 1220329279925658): every failure is reported and the old file stays until the new one is complete.

namespace {
	fs::path make_dir(const std::string& name) {
		const fs::path dir = fs::temp_directory_path() / "hivewe_map_archive_export_test" / name;
		std::error_code ec;
		if (fs::exists(dir, ec)) {
			for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
				std::error_code ignored;
				fs::permissions(it->path(), fs::perms::owner_write, fs::perm_options::add, ignored);
			}
		}
		fs::remove_all(dir, ec);
		fs::create_directories(dir, ec);
		return dir;
	}

	void write_all(const fs::path& path, const std::string& content) {
		std::error_code ec;
		fs::create_directories(path.parent_path(), ec);
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		out << content;
	}

	std::string read_all(const fs::path& path) {
		std::ifstream in(path, std::ios::binary);
		return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
	}

	/// A small map folder: war3map.w3i at the root and scripts\war3map.j below it.
	fs::path make_map(const std::string& name) {
		const fs::path folder = make_dir(name) / "map";
		write_all(folder / "war3map.w3i", std::string(300, 'i'));
		write_all(folder / "scripts" / "war3map.j", "function main takes nothing returns nothing\nendfunction\n");
		return folder;
	}

	std::optional<std::string> read_archived(const fs::path& archive, const std::string& name) {
		HANDLE handle = nullptr;
		if (!SFileOpenArchive(archive.c_str(), 0, MPQ_OPEN_READ_ONLY, &handle)) {
			return std::nullopt;
		}
		std::optional<std::string> result;
		HANDLE file = nullptr;
		if (SFileOpenFileEx(handle, name.c_str(), SFILE_OPEN_FROM_MPQ, &file)) {
			const DWORD size = SFileGetFileSize(file, nullptr);
			std::string bytes(size, '\0');
			DWORD read = 0;
			if (SFileReadFile(file, bytes.data(), size, &read, nullptr) && read == size) {
				result = bytes;
			}
			SFileCloseFile(file);
		}
		SFileCloseArchive(handle);
		return result;
	}

	/// True when no `*.hivewe-tmp*` file is left in `folder`.
	bool no_temp_files(const fs::path& folder) {
		std::error_code ec;
		for (auto it = fs::directory_iterator(folder, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
			if (it->path().filename().native().find(L".hivewe-tmp") != std::wstring::npos) {
				return false;
			}
		}
		return !ec;
	}

	/// Holds a file open with the given share mode until destroyed.
	struct HeldFile {
		HANDLE handle = INVALID_HANDLE_VALUE;
		HeldFile(const fs::path& path, DWORD access, DWORD share) {
			handle = CreateFileW(path.c_str(), access, share, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		}
		~HeldFile() {
			if (handle != INVALID_HANDLE_VALUE) {
				CloseHandle(handle);
			}
		}
		HeldFile(const HeldFile&) = delete;
		HeldFile& operator=(const HeldFile&) = delete;
	};

	/// The archive step as it was before this change (hivewe.cpp write_map_archive), kept to compare the bytes of a successful export.
	void legacy_write(const fs::path& folder, const fs::path& file_name) {
		fs::remove(file_name);
		const uint64_t file_count = std::distance(fs::recursive_directory_iterator { folder }, {});
		HANDLE handle = nullptr;
		REQUIRE(SFileCreateArchive(file_name.c_str(), MPQ_CREATE_LISTFILE | MPQ_CREATE_ATTRIBUTES, static_cast<DWORD>(file_count), &handle));
		for (const auto& entry : fs::recursive_directory_iterator(folder)) {
			if (entry.is_regular_file() && !is_map_write_temp_file(entry.path())) {
				const std::u8string text = entry.path().lexically_relative(folder).u8string();
				const std::string archive_name(text.begin(), text.end());
				SFileAddFileEx(handle, entry.path().c_str(), archive_name.c_str(), MPQ_FILE_COMPRESS, MPQ_COMPRESSION_ZLIB, MPQ_COMPRESSION_NEXT_SAME);
			}
		}
		SFileCompactArchive(handle, nullptr, false);
		SFileCloseArchive(handle);
	}

	/// Checks the failure contract: not ok, the step, the old target bytes unchanged and no temporary file left.
	void check_failed(const map_archive::Result& result, const Step step, const fs::path& target, const std::optional<std::string>& old_bytes) {
		CHECK_FALSE(result.ok);
		CHECK(result.step == step);
		CHECK(result.leftover.empty());
		CHECK(no_temp_files(target.parent_path()));
		if (old_bytes) {
			CHECK(result.target_existed);
			CHECK(read_all(target) == *old_bytes);
		} else {
			CHECK_FALSE(result.target_existed);
			CHECK_FALSE(fs::exists(target));
		}
	}
}

TEST_CASE("map archive export: two files are packed and named in the listfile") {
	const fs::path folder = make_map("two_files");
	const fs::path target = folder.parent_path() / "out.w3x";
	const auto result = map_archive::write(folder, target);
	CHECK(result.ok);
	CHECK(result.step == Step::none);
	CHECK(result.added == 2);
	CHECK(result.failed.empty());
	CHECK_FALSE(result.target_existed);
	CHECK(read_archived(target, "war3map.w3i") == std::string(300, 'i'));
	CHECK(read_archived(target, "scripts\\war3map.j") == read_all(folder / "scripts" / "war3map.j"));
	const auto listfile = read_archived(target, "(listfile)");
	REQUIRE(listfile.has_value());
	CHECK(listfile->find("war3map.w3i") != std::string::npos);
	CHECK(listfile->find("scripts\\war3map.j") != std::string::npos);
	CHECK(no_temp_files(target.parent_path()));
}

TEST_CASE("map archive export: an existing archive is replaced") {
	const fs::path folder = make_map("replace_ok");
	const fs::path target = folder.parent_path() / "out.w3x";
	write_all(target, "OLD ARCHIVE");
	const auto result = map_archive::write(folder, target);
	CHECK(result.ok);
	CHECK(result.target_existed);
	CHECK(read_archived(target, "war3map.w3i") == std::string(300, 'i'));
	CHECK(no_temp_files(target.parent_path()));
}

TEST_CASE("map archive export: a successful export has the same bytes as before the change") {
	const fs::path folder = make_map("bytes");
	write_all(folder / "war3map.wts", "STRING 1\n{\nhello\n}\n");
	write_all(folder / "war3mapImported" / "a.blp", std::string(5000, 'b'));
	write_all(folder / "war3map.doo.hivewe-tmp", "a crashed save's leftover");
	const fs::path legacy = folder.parent_path() / "legacy.w3x";
	const fs::path current = folder.parent_path() / "current.w3x";
	legacy_write(folder, legacy);
	const auto result = map_archive::write(folder, current);
	REQUIRE(result.ok);
	CHECK(result.added == 4);
	const std::string legacy_bytes = read_all(legacy);
	CHECK(legacy_bytes.size() > 0);
	CHECK(read_all(current) == legacy_bytes);
}

TEST_CASE("map archive export: the 3.0 map files are packed byte for byte") {
	const fs::path folder = make_map("own_files");
	const std::map<std::string, std::string> files = {
		{ "war3map.w3l", std::string("W3L!\x03\0\0\0", 8) },
		{ "war3map.w3grp", std::string(12, '\0') },
		{ "war3mapPostProcessing.txt", "[ASSAO]\r\nRadius=6.000000\r\n" },
	};
	for (const auto& [name, content] : files) {
		write_all(folder / name, content);
	}
	const fs::path target = folder.parent_path() / "out.w3x";
	const auto result = map_archive::write(folder, target);
	REQUIRE(result.ok);
	for (const auto& [name, content] : files) {
		CAPTURE(name);
		CHECK(read_archived(target, name) == content);
	}
}

TEST_CASE("map archive export: root-level archive internal names in the folder are skipped") {
	const fs::path folder = make_map("internal_names");
	write_all(folder / "(listfile)", "JUNK LISTFILE");
	write_all(folder / "(Attributes)", "JUNK");
	write_all(folder / "(SIGNATURE)", "JUNK");
	const fs::path target = folder.parent_path() / "out.w3x";
	const auto result = map_archive::write(folder, target);
	CHECK(result.ok);
	CHECK(result.added == 2);
	CHECK(result.failed.empty());
	const auto listfile = read_archived(target, "(listfile)");
	REQUIRE(listfile.has_value());
	CHECK(listfile->find("JUNK") == std::string::npos);
	CHECK(listfile->find("war3map.w3i") != std::string::npos);
}

TEST_CASE("map archive export: a file that cannot be added fails the export and keeps the old archive") {
	const fs::path folder = make_map("add_failure");
	write_all(folder / "locked.txt", "held open");
	const fs::path target = folder.parent_path() / "out.w3x";
	write_all(target, "OLD ARCHIVE");
	map_archive::Result result;
	{
		const HeldFile held(folder / "locked.txt", GENERIC_READ, 0);
		REQUIRE(held.handle != INVALID_HANDLE_VALUE);
		result = map_archive::write(folder, target);
	}
	check_failed(result, Step::add, target, "OLD ARCHIVE");
	REQUIRE(result.failed.size() == 1);
	CHECK(result.failed[0].name == "locked.txt");
	CHECK(result.failed[0].error != 0);
	CHECK(result.error == result.failed[0].error);
	CHECK(result.added == 2); // the other files were still tried
}

TEST_CASE("map archive export: an archive that cannot be created fails without writing") {
	const fs::path folder = make_map("create_failure");
	const fs::path target = folder.parent_path() / "missing_folder" / "out.w3x";
	const auto result = map_archive::write(folder, target);
	CHECK_FALSE(result.ok);
	CHECK(result.step == Step::create);
	CHECK(result.error != 0);
	CHECK_FALSE(fs::exists(target.parent_path()));
}

TEST_CASE("map archive export: an archive that cannot replace the held old file keeps the old bytes") {
	const fs::path folder = make_map("replace_failure");
	const fs::path target = folder.parent_path() / "out.w3x";
	write_all(target, "OLD ARCHIVE");
	map_archive::Result result;
	{
		const HeldFile held(target, GENERIC_READ, FILE_SHARE_READ);
		REQUIRE(held.handle != INVALID_HANDLE_VALUE);
		result = map_archive::write(folder, target);
	}
	check_failed(result, Step::replace, target, "OLD ARCHIVE");
	CHECK(result.error != 0);
	CHECK(result.added == 2);
}

TEST_CASE("map archive export: a target inside the map folder is refused before writing") {
	const fs::path folder = make_map("inside");
	for (const fs::path& target : { folder / "out.w3x", folder / "scripts" / "out.w3x" }) {
		const auto result = map_archive::write(folder, target);
		CHECK_FALSE(result.ok);
		CHECK(result.step == Step::inside_map_folder);
		CHECK_FALSE(fs::exists(target));
		CHECK(no_temp_files(target.parent_path()));
	}
}

TEST_CASE("map archive export: a target that is a folder is refused") {
	const fs::path folder = make_map("target_folder");
	const fs::path target = folder.parent_path() / "a_folder.w3x";
	fs::create_directories(target);
	const auto result = map_archive::write(folder, target);
	CHECK_FALSE(result.ok);
	CHECK(result.step == Step::target_is_folder);
	CHECK(fs::is_directory(target));
	CHECK(fs::is_empty(target));
}

TEST_CASE("map archive export: a path too long for the temporary files is refused") {
	const fs::path folder = make_map("long_path");
	const std::size_t parent = fs::absolute(folder.parent_path()).native().size();
	// Short enough for Windows itself, too long once ".hivewe-tmp.tmp" is added.
	const std::size_t name_length = map_archive::max_path_length - parent - 1 - 10;
	REQUIRE(name_length < 250);
	const fs::path target = folder.parent_path() / (std::wstring(name_length - 4, L'x') + L".w3x");
	const auto result = map_archive::write(folder, target);
	CHECK_FALSE(result.ok);
	CHECK(result.step == Step::path_too_long);
	CHECK_FALSE(fs::exists(target));
	CHECK(no_temp_files(target.parent_path()));
}

TEST_CASE("map archive export: a read-only existing file is refused and kept read-only") {
	const fs::path folder = make_map("read_only_target");
	const fs::path target = folder.parent_path() / "out.w3x";
	write_all(target, "OLD ARCHIVE");
	fs::permissions(target, fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write, fs::perm_options::remove);
	const auto result = map_archive::write(folder, target);
	CHECK_FALSE(result.ok);
	CHECK(result.step == Step::target_read_only);
	CHECK(map_archive::is_refusal(result.step));
	CHECK(result.target_existed);
	CHECK(read_all(target) == "OLD ARCHIVE");
	CHECK((fs::status(target).permissions() & fs::perms::owner_write) == fs::perms::none);
	CHECK(no_temp_files(target.parent_path()));
	fs::permissions(target, fs::perms::owner_write, fs::perm_options::add);
}

TEST_CASE("map archive export: non-ASCII target path and file name") {
	const fs::path folder = make_map("unicode");
	const fs::path file = folder / L"Übersicht_地图.txt";
	write_all(file, "unicode content");
	const fs::path target_folder = folder.parent_path() / L"Карта";
	fs::create_directories(target_folder);
	const fs::path target = target_folder / L"地图.w3x";
	const auto result = map_archive::write(folder, target);
	CHECK(result.ok);
	CHECK(result.added == 3);
	const std::u8string name = file.filename().u8string();
	CHECK(read_archived(target, std::string(name.begin(), name.end())) == "unicode content");
}

TEST_CASE("map archive export: a stale temporary file is removed first, or fails the export when it cannot be") {
	const fs::path folder = make_map("stale_temp");
	const fs::path target = folder.parent_path() / "out.w3x";
	fs::path temp = target;
	temp += ".hivewe-tmp";
	write_all(temp, "LEFT BY A CRASH");
	fs::permissions(temp, fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write, fs::perm_options::remove);
	const auto removed = map_archive::write(folder, target);
	CHECK(removed.ok);
	CHECK(read_archived(target, "war3map.w3i").has_value());
	CHECK(no_temp_files(target.parent_path()));

	write_all(target, "OLD ARCHIVE");
	write_all(temp, "LEFT BY A CRASH");
	map_archive::Result held_result;
	{
		const HeldFile held(temp, GENERIC_READ, 0);
		REQUIRE(held.handle != INVALID_HANDLE_VALUE);
		held_result = map_archive::write(folder, target);
	}
	CHECK_FALSE(held_result.ok);
	CHECK(held_result.step == Step::stale_temp);
	CHECK(held_result.error != 0);
	CHECK(read_all(target) == "OLD ARCHIVE");
	CHECK(read_all(temp) == "LEFT BY A CRASH"); // not ours to destroy half-way
	fs::remove(temp);
}

TEST_CASE("map archive export: an empty map folder is a failure") {
	const fs::path folder = make_dir("empty") / "map";
	fs::create_directories(folder);
	write_all(folder / "war3map.w3e.hivewe-tmp", "only a leftover");
	const fs::path target = folder.parent_path() / "out.w3x";
	const auto result = map_archive::write(folder, target);
	check_failed(result, Step::nothing_added, target, std::nullopt);
	CHECK(result.added == 0);
}

TEST_CASE("map archive export: flush, compact and close failures keep the old archive") {
	const auto failing = [](const DWORD error) {
		return [error](void*) {
			SetLastError(error);
			return false;
		};
	};

	{
		const fs::path folder = make_map("flush_failure");
		const fs::path target = folder.parent_path() / "out.w3x";
		write_all(target, "OLD ARCHIVE");
		bool compact_called = false;
		map_archive::Steps steps;
		steps.flush = failing(ERROR_DISK_FULL);
		steps.compact = [&](void* handle) {
			compact_called = true;
			return static_cast<bool>(SFileCompactArchive(handle, nullptr, false));
		};
		const auto result = map_archive::write(folder, target, steps);
		check_failed(result, Step::flush, target, "OLD ARCHIVE");
		CHECK(result.error == ERROR_DISK_FULL);
		CHECK_FALSE(compact_called); // compact would swallow the failed listfile write
	}
	{
		const fs::path folder = make_map("compact_failure");
		const fs::path target = folder.parent_path() / "out.w3x";
		write_all(target, "OLD ARCHIVE");
		map_archive::Steps steps;
		steps.compact = failing(ERROR_WRITE_FAULT);
		const auto result = map_archive::write(folder, target, steps);
		check_failed(result, Step::compact, target, "OLD ARCHIVE");
		CHECK(result.error == ERROR_WRITE_FAULT);
	}
	{
		const fs::path folder = make_map("close_failure");
		const fs::path target = folder.parent_path() / "out.w3x";
		map_archive::Steps steps;
		steps.close = [](void* handle) {
			SFileCloseArchive(handle);
			SetLastError(ERROR_HANDLE_DISK_FULL);
			return false;
		};
		const auto result = map_archive::write(folder, target, steps);
		check_failed(result, Step::close, target, std::nullopt);
		CHECK(result.error == ERROR_HANDLE_DISK_FULL);
	}
	{
		// Close is still called after an earlier failure, so the temporary file can be deleted.
		const fs::path folder = make_map("close_after_failure");
		const fs::path target = folder.parent_path() / "out.w3x";
		bool closed = false;
		map_archive::Steps steps;
		steps.compact = failing(ERROR_WRITE_FAULT);
		steps.close = [&](void* handle) {
			closed = true;
			return static_cast<bool>(SFileCloseArchive(handle));
		};
		const auto result = map_archive::write(folder, target, steps);
		check_failed(result, Step::compact, target, std::nullopt);
		CHECK(closed);
	}
}

TEST_CASE("path_is_within") {
	const fs::path folder = make_dir("within") / "Map";
	fs::create_directories(folder);
	CHECK(path_is_within(folder, folder));
	CHECK(path_is_within(folder / "a" / "b.w3x", folder));
	CHECK(path_is_within(folder.parent_path() / "MAP" / "x.w3x", folder));
	CHECK_FALSE(path_is_within(folder.parent_path() / "Map.w3x", folder));
	CHECK_FALSE(path_is_within(folder.parent_path() / "Map2" / "x.w3x", folder));
}
