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
