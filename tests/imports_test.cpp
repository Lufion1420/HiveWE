#include <string>
#include <doctest/doctest.h>

import std;
import Hierarchy;
import Imports;

namespace fs = std::filesystem;

// Imports::save lists the map folder into war3map.imp. path::string() throws on MSVC for file names outside the system code page, which
// aborted every save of a map that holds such an imported file.

namespace {
	void touch(const fs::path& path) {
		fs::create_directories(path.parent_path());
		std::ofstream(path, std::ios::binary) << "x";
	}

	std::string read_all(const fs::path& path) {
		std::ifstream in(path, std::ios::binary);
		return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
	}

	std::string utf8(const std::u8string& text) {
		return std::string(text.begin(), text.end());
	}
}

TEST_CASE("Imports::save does not list the 3.0 map files, also when an older war3map.imp did, and leaves them as they are") {
	const fs::path map = fs::temp_directory_path() / "hivewe_imports_test" / "own_files";
	std::error_code ec;
	fs::remove_all(map, ec);
	touch(map / "war3mapImported" / "a.blp");

	const fs::path original = hierarchy.map_directory;
	hierarchy.map_directory = map;
	const auto saved_imp = [&]() {
		Imports().save(map);
		return read_all(map / "war3map.imp");
	};

	// Without the new files: version 1, one entry (the byte 13 and the NUL terminated path).
	const std::string expected = std::string("\x01\0\0\0\x01\0\0\0\x0D", 9) + "war3mapImported\\a.blp" + std::string(1, '\0');
	CHECK(saved_imp() == expected);

	const std::map<std::string, std::string> own_files = {
		{ "war3map.w3l", std::string("W3L!\x03\0\0\0", 8) },
		{ "war3map.w3grp", std::string(12, '\0') },
		{ "war3mapPostProcessing.txt", "[ASSAO]\r\nRadius=6.000000\r\n" },
	};
	for (const auto& [name, content] : own_files) {
		std::ofstream(map / name, std::ios::binary) << content;
	}
	// An older HiveWE listed them as imports: the next save writes the list without them.
	std::ofstream(map / "war3map.imp", std::ios::binary | std::ios::trunc)
		<< std::string("\x01\0\0\0\x01\0\0\0\x0D", 9) + "war3map.w3l" + std::string(1, '\0');

	CHECK(saved_imp() == expected);
	for (const auto& [name, content] : own_files) {
		CAPTURE(name);
		CHECK(read_all(map / name) == content);
	}

	hierarchy.map_directory = original;
}

TEST_CASE("Imports::save lists imported files with non-code-page names as UTF-8 and keeps ASCII names unchanged") {
	const fs::path map = fs::temp_directory_path() / "hivewe_imports_test" / fs::path(u8"地图_КАРТА");
	std::error_code ec;
	fs::remove_all(map.parent_path(), ec);
	touch(map / "war3map.w3e");            // blacklisted: not an import
	touch(map / "war3map.w3i");            // blacklisted
	touch(map / "Units" / "plain.mdx");    // ASCII import in a folder
	touch(map / fs::path(u8"текстура.blp"));
	touch(map / fs::path(u8"贴图") / fs::path(u8"贴图.blp"));
	touch(map / "leftover.blp.hivewe-tmp"); // a crashed save's temporary file: not an import

	const fs::path original = hierarchy.map_directory;
	hierarchy.map_directory = map;
	CHECK_NOTHROW(Imports().save(map));
	hierarchy.map_directory = original;

	const std::string imp = read_all(map / "war3map.imp");
	REQUIRE(imp.size() > 8);

	// Header: version 1, then the entry count.
	std::uint32_t version = 0;
	std::uint32_t count = 0;
	std::memcpy(&version, imp.data(), 4);
	std::memcpy(&count, imp.data() + 4, 4);
	CHECK(version == 1);
	CHECK(count == 3);

	// Each entry is the byte 13 and a NUL-terminated relative path, in folder iteration order.
	std::set<std::string> names;
	std::size_t position = 8;
	while (position < imp.size()) {
		REQUIRE(imp[position] == 13);
		const std::size_t end = imp.find('\0', position + 1);
		REQUIRE(end != std::string::npos);
		names.insert(imp.substr(position + 1, end - position - 1));
		position = end + 1;
	}
	CHECK(names == std::set<std::string> {
		"Units\\plain.mdx",
		utf8(u8"текстура.blp"),
		utf8(u8"贴图\\贴图.blp"),
	});
}
