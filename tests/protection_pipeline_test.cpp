#include <doctest/doctest.h>

import std;
import AssetObfuscation;
import ProtectionPipeline;

namespace fs = std::filesystem;

namespace {
	fs::path make_scratch_dir(const std::string& name) {
		const fs::path dir = fs::temp_directory_path() / "hivewe_protection_pipeline_test" / name;
		std::error_code ec;
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
}

TEST_CASE("escape_script_string / unescape_script_string round-trip") {
	const std::vector<std::string> samples = {
		"war3mapImported/Custom.mdx",
		"war3mapImported\\Custom.mdx",
		"quote\"backslash\\newline\nend",
		"",
	};
	for (const std::string& sample : samples) {
		CHECK(unescape_script_string(escape_script_string(sample)) == sample);
	}
}

TEST_CASE("unescape_script_string turns an escaped backslash path into a real single-backslash path") {
	// This is what a JASS/Lua compiler actually emits for a Windows-style path literal: each real
	// backslash becomes the two source characters \\ inside the quotes.
	const std::string raw_source_literal = "war3mapImported\\\\Custom.mdx";
	CHECK(unescape_script_string(raw_source_literal) == "war3mapImported\\Custom.mdx");
	CHECK(asset_match_key(unescape_script_string(raw_source_literal)) == asset_match_key("war3mapImported/Custom.mdx"));
}

TEST_CASE("rewrite_script_asset_references rewrites a matching path literal in war3map.j") {
	const fs::path dir = make_scratch_dir("rewrite_script_j");
	// war3mapImported\\Custom.mdx in the actual JASS source text below is the two-character-escaped
	// form of a single backslash - exactly what a real compiled script contains.
	write_all(dir / "war3map.j", "function Init takes nothing returns nothing\n"
								  "    call AddSpecialEffect(\"war3mapImported\\\\Custom.mdx\", 0, 0)\n"
								  "    call DisplayTextToPlayer(GetLocalPlayer(), 0, 0, \"not a path\")\n"
								  "endfunction\n");

	RenameCandidate candidate;
	candidate.original_relative_path = "war3mapImported/Custom.mdx";
	candidate.new_relative_path = "a3f9c1e2.mdx";
	candidate.match_key = asset_match_key("war3mapImported/Custom.mdx");

	const AssetObfuscationResult result = rewrite_script_asset_references(dir, { candidate });
	CHECK(result.success);

	const std::string rewritten = read_all(dir / "war3map.j");
	CHECK(rewritten.find("a3f9c1e2.mdx") != std::string::npos);
	CHECK(rewritten.find("war3mapImported") == std::string::npos);
	CHECK(rewritten.find("not a path") != std::string::npos); // unrelated literal left alone
}

TEST_CASE("rewrite_script_asset_references leaves war3map.lua untouched when nothing matches") {
	const fs::path dir = make_scratch_dir("rewrite_script_lua_no_match");
	const std::string original = "print(\"hello world\")\nAddSpecialEffect(\"Doodads/Other.mdx\", 0, 0)\n";
	write_all(dir / "war3map.lua", original);

	RenameCandidate candidate;
	candidate.original_relative_path = "war3mapImported/Custom.mdx";
	candidate.new_relative_path = "a3f9c1e2.mdx";
	candidate.match_key = asset_match_key("war3mapImported/Custom.mdx");

	const AssetObfuscationResult result = rewrite_script_asset_references(dir, { candidate });
	CHECK(result.success);
	CHECK(read_all(dir / "war3map.lua") == original);
}

TEST_CASE("rewrite_script_asset_references rewrites a single-quoted Lua literal") {
	// Real case: a map's spell-kit Lua code used single quotes for these calls
	// (EffectEx():onPoint('NCOW_SFX_Whatever.mdx', x, y, 0)), which the scanner originally missed
	// entirely since it only recognized double-quoted strings.
	const fs::path dir = make_scratch_dir("rewrite_script_lua_single_quote");
	write_all(dir / "war3map.lua", "EffectEx():onPoint('war3mapImported/Custom.mdx',x,y,0):setScale(2)\n");

	RenameCandidate candidate;
	candidate.original_relative_path = "war3mapImported/Custom.mdx";
	candidate.new_relative_path = "a3f9c1e2.mdx";
	candidate.match_key = asset_match_key("war3mapImported/Custom.mdx");

	const AssetObfuscationResult result = rewrite_script_asset_references(dir, { candidate });
	CHECK(result.success);

	const std::string rewritten = read_all(dir / "war3map.lua");
	CHECK(rewritten.find("a3f9c1e2.mdx") != std::string::npos);
	CHECK(rewritten.find("war3mapImported") == std::string::npos);
}

TEST_CASE("rewrite_script_asset_references leaves a JASS rawcode literal alone even if it happens to match") {
	// 'hfoo' in JASS is not a string - it's a 4-character rawcode literal that compiles to an
	// integer constant. find_string_literals() must never treat single-quoted JASS content as a
	// string (unlike Lua, where single and double quotes are interchangeable), or this would both
	// misclassify a rawcode and risk corrupting the script if a rewrite ever fired on it.
	const fs::path dir = make_scratch_dir("rewrite_script_jass_rawcode");
	const std::string original = "call BlzSetAbilityIcon('hfoo', \"war3mapImported\\\\Unrelated.blp\")\n";
	write_all(dir / "war3map.j", original);

	RenameCandidate candidate;
	candidate.original_relative_path = "hfoo";
	candidate.new_relative_path = "a3f9c1e2.hfoo";
	candidate.match_key = asset_match_key("hfoo");

	const AssetObfuscationResult result = rewrite_script_asset_references(dir, { candidate });
	CHECK(result.success);
	CHECK(read_all(dir / "war3map.j") == original); // 'hfoo' must survive untouched
}

TEST_CASE("rewrite_script_asset_references does not touch a quote inside a comment") {
	const fs::path dir = make_scratch_dir("rewrite_script_comment");
	const std::string original = "-- AddSpecialEffect(\"war3mapImported\\\\Custom.mdx\", 0, 0)\nprint(\"unrelated\")\n";
	write_all(dir / "war3map.lua", original);

	RenameCandidate candidate;
	candidate.original_relative_path = "war3mapImported/Custom.mdx";
	candidate.new_relative_path = "a3f9c1e2.mdx";
	candidate.match_key = asset_match_key("war3mapImported/Custom.mdx");

	const AssetObfuscationResult result = rewrite_script_asset_references(dir, { candidate });
	CHECK(result.success);
	CHECK(read_all(dir / "war3map.lua") == original); // commented-out reference must not be rewritten
}

TEST_CASE("is_stormlib_fabricated_name matches StormLib's reserved no-name placeholder pattern") {
	// Confirmed directly against a real listfile-less archive: SFileAddFileEx rejects re-adding a
	// file under any name matching this pattern with ERROR_INVALID_PARAMETER, regardless of the
	// file's actual content or which 8 digits/extension are used.
	CHECK(is_stormlib_fabricated_name("File00000000.xxx"));
	CHECK(is_stormlib_fabricated_name("File99999999.xxx"));
	CHECK(is_stormlib_fabricated_name("File00003784.blp"));

	// Real, legitimately-named files must never be caught by this.
	CHECK_FALSE(is_stormlib_fabricated_name("war3map.lua"));
	CHECK_FALSE(is_stormlib_fabricated_name("Filename.blp")); // "Filename", not "File" + 8 digits
	CHECK_FALSE(is_stormlib_fabricated_name("File1234567.blp")); // only 7 digits
	CHECK_FALSE(is_stormlib_fabricated_name("File123456789.blp")); // 9 digits
	CHECK_FALSE(is_stormlib_fabricated_name("File00000000")); // no extension at all
	CHECK_FALSE(is_stormlib_fabricated_name("File00000000.")); // dot with nothing after it
}
