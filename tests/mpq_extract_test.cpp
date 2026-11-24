#include <doctest/doctest.h>
#define __STORMLIB_NO_STATIC_LINK__
#include "StormLib.h"

import std;
import MPQ;

namespace fs = std::filesystem;

namespace {
	fs::path make_dir(const std::string& name) {
		const fs::path dir = fs::temp_directory_path() / "hivewe_mpq_extract_test" / name;
		std::error_code ec;
		fs::remove_all(dir, ec);
		fs::create_directories(dir, ec);
		return dir;
	}

	/// A small archive built through the MPQ class: hello.txt and sub/inner.txt.
	fs::path make_archive(const std::string& name) {
		const fs::path archive = make_dir(name) / "test.mpq";
		mpq::MPQ writer;
		REQUIRE(SFileCreateArchive(archive.c_str(), MPQ_CREATE_ARCHIVE_V1, 16, &writer.handle));
		writer.file_write("hello.txt", { 'h', 'i' });
		writer.file_write("sub\\inner.txt", { 'i', 'n' });
		REQUIRE(SFileFlushArchive(writer.handle));
		writer.close();
		return archive;
	}

	std::string read_text(const fs::path& path) {
		std::ifstream in(path, std::ios::binary);
		return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
	}
}

TEST_CASE("extract_file reports extracted for a present file") {
	const fs::path archive = make_archive("present");
	mpq::MPQ opened;
	REQUIRE(opened.open(archive));
	const fs::path out = make_dir("present_out") / "deep" / "hello.txt";
	CHECK(opened.extract_file("hello.txt", out) == mpq::MPQ::ExtractResult::extracted);
	CHECK(read_text(out) == "hi");
}

TEST_CASE("extract_file reports absent, not failed, for a missing file") {
	const fs::path archive = make_archive("absent");
	mpq::MPQ opened;
	REQUIRE(opened.open(archive));
	const fs::path out = make_dir("absent_out") / "nothere.txt";
	CHECK(opened.extract_file("nothere.txt", out) == mpq::MPQ::ExtractResult::absent);
	CHECK_FALSE(fs::exists(out));
}

TEST_CASE("extract_file reports failed when the target cannot be written") {
	const fs::path archive = make_archive("failed");
	mpq::MPQ opened;
	REQUIRE(opened.open(archive));
	const fs::path dir = make_dir("failed_out");

	// A directory sits where the file should go.
	fs::create_directories(dir / "hello.txt");
	CHECK(opened.extract_file("hello.txt", dir / "hello.txt") == mpq::MPQ::ExtractResult::failed);

	// A regular file sits where a parent folder should go.
	{
		std::ofstream(dir / "blocker") << "x";
	}
	CHECK(opened.extract_file("hello.txt", dir / "blocker" / "hello.txt") == mpq::MPQ::ExtractResult::failed);
}

TEST_CASE("unpack extracts every file and returns true") {
	const fs::path archive = make_archive("unpack_ok");
	mpq::MPQ opened;
	REQUIRE(opened.open(archive));
	const fs::path out = make_dir("unpack_ok_out");
	std::vector<std::string> failed;
	CHECK(opened.unpack(out, &failed));
	CHECK(failed.empty());
	CHECK(read_text(out / "hello.txt") == "hi");
	CHECK(read_text(out / "sub" / "inner.txt") == "in");
	CHECK_FALSE(fs::exists(out / "(listfile)"));
}

TEST_CASE("unpack returns false and names the entry that failed, but still extracts the rest and cleans up") {
	const fs::path archive = make_archive("unpack_fail");
	mpq::MPQ opened;
	REQUIRE(opened.open(archive));
	const fs::path out = make_dir("unpack_fail_out");
	fs::create_directories(out / "hello.txt"); // a directory blocks the target file
	std::vector<std::string> failed;
	CHECK_FALSE(opened.unpack(out, &failed));
	REQUIRE(failed.size() == 1);
	CHECK(failed[0] == "hello.txt");
	CHECK(read_text(out / "sub" / "inner.txt") == "in");
	CHECK_FALSE(fs::exists(out / "(listfile)"));

	// The failure also shows without the out-parameter.
	CHECK_FALSE(opened.unpack(out));
}

TEST_CASE("unpack of an archive with no files succeeds") {
	const fs::path archive = make_dir("unpack_empty") / "empty.mpq";
	{
		mpq::MPQ writer;
		REQUIRE(SFileCreateArchive(archive.c_str(), MPQ_CREATE_ARCHIVE_V1, 16, &writer.handle));
		REQUIRE(SFileFlushArchive(writer.handle));
		writer.close();
	}
	mpq::MPQ opened;
	REQUIRE(opened.open(archive));
	std::vector<std::string> failed;
	CHECK(opened.unpack(make_dir("unpack_empty_out"), &failed));
	CHECK(failed.empty());
}

TEST_CASE("is_safe_archive_name refuses names that can leave the target folder") {
	for (const char* name : { "", "..\\evil.txt", "../evil.txt", "sub\\..\\..\\evil.txt", "sub/../../evil.txt", "..", "C:\\evil.txt", "C:evil.txt",
							  "\\evil.txt", "/evil.txt", "\\\\server\\share\\evil.txt", "file.txt:stream",
							  "a\\\\b.txt", "a\\b\\", ".", "a\\.\\b.txt", " ", "a\\ . \\b.txt", "CON", "con.txt", "a\\NUL", "a\\Prn.dat", "aux .txt", "COM1",
							  "com9.blp", "LPT5.x", "CONIN$", "conout$.txt", "sub\\CON\\x.txt",
							  "COM\xB9", "com\xB2.txt", "LPT\xB3", "lpt\xB9.x", "COM\xC2\xB9", "com\xC2\xB2.txt", "LPT\xC2\xB3", "a\\LPT\xC2\xB9.x" }) {
		CAPTURE(name);
		CHECK_FALSE(mpq::is_safe_archive_name(name));
	}
	for (const char* name : { "war3map.w3i", "a\\b\\c.txt", "a/b/c.txt", "war3mapImported\\x.blp", "File00001234.blp", "..hidden.txt",
							  "(listfile)", "(1)name.w3x", "a b\\(c).txt", "console.txt", "COM0", "COM10", "LPT", "communist.txt", "nullable.mdx", "COM\xB9\xB9", "COM\xC2\xB4" }) {
		CAPTURE(name);
		CHECK(mpq::is_safe_archive_name(name));
	}
}

TEST_CASE("extract_file refuses a stored hostile name, skips an unstored one as absent, and writes nothing") {
	const fs::path parent = make_dir("hostile_extract_out");
	const fs::path target = parent / "target";
	fs::create_directories(target);
	std::error_code ignored;
	fs::remove(parent.parent_path() / "evil.txt", ignored);
	// Absolute names point into the test's own folder, so a regression stays inside it and is seen.
	const std::string absolute = (parent / "abs_evil.txt").string();
	const std::vector<std::string> hostile = { "..\\evil.txt", "sub\\..\\..\\evil.txt", absolute, "CON", "a\\NUL.txt" };

	const fs::path archive = make_dir("hostile_extract") / "hostile.mpq";
	{
		mpq::MPQ writer;
		REQUIRE(SFileCreateArchive(archive.c_str(), MPQ_CREATE_ARCHIVE_V1, 32, &writer.handle));
		for (const std::string& name : hostile) {
			writer.file_write(name, { 'x' });
		}
		REQUIRE(SFileFlushArchive(writer.handle));
		writer.close();
	}
	mpq::MPQ opened;
	REQUIRE(opened.open(archive));

	std::size_t stored = 0;
	for (const std::string& name : hostile) {
		CAPTURE(name);
		const bool is_stored = opened.file_exists(name);
		stored += is_stored;
		CHECK(opened.extract_file(name, target / "out.txt")
			  == (is_stored ? mpq::MPQ::ExtractResult::failed : mpq::MPQ::ExtractResult::absent));
	}
	REQUIRE(stored > 0);

	// Unsafe names the archive does not hold are skipped as before (a stale import-list entry).
	for (const char* name : { "\\stale.txt", "/stale.txt", "C:\\stale.txt", "..\\stale.txt", "NUL" }) {
		CAPTURE(name);
		CHECK(opened.extract_file(name, target / "out.txt") == mpq::MPQ::ExtractResult::absent);
	}
	CHECK_FALSE(fs::exists(parent / "evil.txt"));
	CHECK_FALSE(fs::exists(parent / "abs_evil.txt"));
	CHECK_FALSE(fs::exists(parent.parent_path() / "evil.txt"));
	CHECK(fs::is_empty(target));
}

TEST_CASE("unpack refuses hostile entries, writes nothing outside the target and still extracts the benign ones") {
	const fs::path parent = make_dir("hostile_unpack_out");
	const fs::path target = parent / "target";
	fs::create_directories(target);
	// An old leftover must not poison the escape checks below.
	std::error_code ignored;
	fs::remove(parent.parent_path() / "evil.txt", ignored);
	fs::remove(parent / "evil.txt", ignored);
	fs::remove(parent / "abs_evil.txt", ignored);
	// Absolute name inside the test's own folder, so a regression stays there and is seen.
	const std::string absolute = (parent / "abs_evil.txt").string();
	const std::vector<std::string> hostile = { "..\\evil.txt", "sub\\..\\..\\evil.txt", absolute, "CON" };

	const fs::path archive = make_dir("hostile_unpack") / "hostile.mpq";
	{
		mpq::MPQ writer;
		REQUIRE(SFileCreateArchive(archive.c_str(), MPQ_CREATE_ARCHIVE_V1, 32, &writer.handle));
		for (const std::string& name : hostile) {
			writer.file_write(name, { 'x' });
		}
		writer.file_write("a\\b\\c.txt", { 'x' });
		REQUIRE(SFileFlushArchive(writer.handle));
		writer.close();
	}
	mpq::MPQ opened;
	REQUIRE(opened.open(archive));

	// Which hostile names StormLib really stored; the refusal must name exactly those.
	std::vector<std::string> stored;
	for (const std::string& name : hostile) {
		if (opened.file_exists(name)) {
			stored.push_back(name);
		}
	}
	INFO("hostile names stored by StormLib: ", stored.size(), " of ", hostile.size());
	REQUIRE_FALSE(stored.empty());

	std::vector<std::string> failed;
	const bool ok = opened.unpack(target, &failed);
	REQUIRE_FALSE(ok);
	std::ranges::sort(stored);
	std::ranges::sort(failed);
	CHECK(failed == stored);

	CHECK_FALSE(fs::exists(parent / "evil.txt"));
	CHECK_FALSE(fs::exists(parent / "abs_evil.txt"));
	CHECK_FALSE(fs::exists(parent.parent_path() / "evil.txt"));
	CHECK_FALSE(fs::exists(target / "CON"));
	CHECK(read_text(target / "a" / "b" / "c.txt") == "x");
}
