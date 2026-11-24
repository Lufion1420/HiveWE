#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdio>
#include <doctest/doctest.h>

import MDX;
import BinaryReader;

namespace {
	std::string read_text_file(const std::filesystem::path& path) {
		std::ifstream f(path, std::ios::binary);
		REQUIRE(f.is_open());
		std::stringstream ss;
		ss << f.rdbuf();
		return ss.str();
	}

	/// MDL (file) -> MDX (memory) -> MDL (file)
	void roundtrip_fixture_mdl(const char* relative_path) {
		const std::filesystem::path fixture = std::filesystem::path(MDL_FIXTURES_DIR) / relative_path;
		const std::string source = read_text_file(fixture);

		// 1st pass: parse the fixture, then write.
		auto parsed_a = mdx::MDX::from_mdl(source);
		REQUIRE(parsed_a.has_value());
		const std::string out_a = parsed_a.value().to_mdl(parsed_a.value().version);

		CHECK_EQ(source.size(), out_a.size());
		CHECK_EQ(std::memcmp(source.data(), out_a.data(), source.size()), 0);
	}

	/// MDL (file) -> MDX (memory) -> MDX (file) -> MDX(memory) -> MDL (file)
	void roundtrip_fixture_mdx(const char* relative_path) {
		const std::filesystem::path fixture = std::filesystem::path(MDL_FIXTURES_DIR) / relative_path;
		const std::string source = read_text_file(fixture);

		auto parsed_a = mdx::MDX::from_mdl(source);
		REQUIRE(parsed_a.has_value());
		auto mdx = parsed_a.value().to_mdx(parsed_a.value().version);

		auto reader = BinaryReader(mdx.buffer);
		auto parsed_b = mdx::MDX(reader);
		auto out_a = parsed_b.to_mdl(parsed_b.version);

		CHECK_EQ(source.size(), out_a.size());
		CHECK_EQ(std::memcmp(source.data(), out_a.data(), source.size()), 0);
	}
}

TEST_CASE("MDL round-trip: minimal_v800.mdl") {
	roundtrip_fixture_mdl("minimal_v800.mdl");
	roundtrip_fixture_mdx("minimal_v800.mdl");
}

TEST_CASE("MDL round-trip: minimal_v900.mdl") {
	roundtrip_fixture_mdl("minimal_v900.mdl");
	roundtrip_fixture_mdx("minimal_v900.mdl");
}

TEST_CASE("MDL round-trip: minimal_v1000.mdl") {
	roundtrip_fixture_mdl("minimal_v1000.mdl");
	roundtrip_fixture_mdx("minimal_v1000.mdl");
}

TEST_CASE("MDL light: a negative static AttenuationStart reads as a negative float and round-trips unchanged") {
	std::string source = read_text_file(std::filesystem::path(MDL_FIXTURES_DIR) / "minimal_v800.mdl");
	const std::string from = "static AttenuationStart 80,";
	const size_t at = source.find(from);
	REQUIRE(at != std::string::npos);
	source.replace(at, from.size(), "static AttenuationStart -5,");

	auto parsed = mdx::MDX::from_mdl(source);
	REQUIRE(parsed.has_value());
	REQUIRE_EQ(parsed.value().lights.size(), 1);
	CHECK_EQ(parsed.value().lights[0].attenuation_start, -5.f);
	CHECK_EQ(parsed.value().lights[0].attenuation_end, 200.f);
	CHECK(parsed.value().to_mdl(parsed.value().version) == source);

	// Through MDX bytes as well
	const auto binary = parsed.value().to_mdx(parsed.value().version);
	auto reader = BinaryReader(binary.buffer);
	auto from_mdx = mdx::MDX(reader);
	CHECK_EQ(from_mdx.lights[0].attenuation_start, -5.f);
	CHECK(from_mdx.to_mdl(from_mdx.version) == source);
}

// Sanity-check the version-aware MDL writer: writing a higher-version model out as
// a lower-version target must strip fields that the lower version does not understand.
TEST_CASE("MDL writer: cross-version downgrade strips version-specific fields") {
	const std::filesystem::path mdl_dir = std::filesystem::path(MDL_FIXTURES_DIR);

	{
		const std::string v900_src = read_text_file(mdl_dir / "minimal_v900.mdl");
		auto parsed = mdx::MDX::from_mdl(v900_src);
		REQUIRE(parsed.has_value());
		const std::string out_v800 = parsed.value().to_mdl(800);
		CHECK(out_v800.find("FormatVersion 800,") != std::string::npos);
		CHECK(out_v800.find("Shader \"") == std::string::npos);
		CHECK(out_v800.find("Tangents ") == std::string::npos);
		CHECK(out_v800.find("SkinWeights ") == std::string::npos);
		CHECK(out_v800.find("BindPose") == std::string::npos);
	}

	{
		const std::string v1000_src = read_text_file(mdl_dir / "minimal_v1000.mdl");
		auto parsed = mdx::MDX::from_mdl(v1000_src);
		REQUIRE(parsed.has_value());
		const std::string out_v900 = parsed.value().to_mdl(900);
		CHECK(out_v900.find("FormatVersion 900,") != std::string::npos);
		CHECK(out_v900.find("EmissiveGain") == std::string::npos);
		CHECK(out_v900.find("FresnelColor") == std::string::npos);
		CHECK(out_v900.find("FresnelAlpha") == std::string::npos);
		CHECK(out_v900.find("FresnelTeamColor") == std::string::npos);
	}
}