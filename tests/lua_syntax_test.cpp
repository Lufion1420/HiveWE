#define DOCTEST_CONFIG_USE_STD_HEADERS
#include <doctest/doctest.h>

import std;
import LuaSyntax;
import ProtectionPipeline;

namespace fs = std::filesystem;

// Lua map scripts are syntax-checked on save with the Lua 5.3 parser (Asana 1220926531963465). Refusing valid code is the worst
// outcome, so the accepted cases come first.

namespace {
	bool parses(const std::string_view source) {
		const std::optional<LuaSyntaxError> error = check_lua_syntax(source);
		if (error) {
			MESSAGE("line ", error->line, ": ", error->message);
		}
		return !error.has_value();
	}

	fs::path make_scratch_dir(const std::string& name) {
		const fs::path dir = fs::temp_directory_path() / "hivewe_lua_syntax_test" / name;
		std::error_code ec;
		fs::remove_all(dir, ec);
		fs::create_directories(dir, ec);
		return dir;
	}

	void write_all(const fs::path& path, const std::string& content) {
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		out << content;
	}

	/// The line of the first syntax error, or -1 when `source` parses.
	int error_line(const std::string_view source) {
		const std::optional<LuaSyntaxError> error = check_lua_syntax(source);
		return error ? error->line : -1;
	}

	bool contains(const std::string_view haystack, const std::string_view needle) {
		return haystack.find(needle) != std::string_view::npos;
	}
} // namespace

TEST_CASE("check_lua_syntax accepts Lua 5.3 syntax the game accepts") {
	CHECK(parses(""));
	CHECK(parses("function main()\nend\n"));
	CHECK(parses("local a = 7 // 2\n"));                                     // integer division, not a comment
	CHECK(parses("local a = (5 & 3) | (1 ~ 2) | (1 << 4) | (256 >> 2) | ~0\n")); // 5.3 bitwise operators
	CHECK(parses("for i = 1, 3 do\n if i == 2 then goto continue end\n ::continue::\nend\n"));
	CHECK(parses("local s = [==[ a ]] \"quote\" ]=] ]==]\n"));
	CHECK(parses("local s = \"\\u{48}\\u{20AC}\\x41\\65\\z\n   end\"\n"));
	CHECK(parses("function main()\r\n\tlocal x = 1\r\nend\r\n"));
	CHECK(parses("local s = \"caf\xE9 \xFF\xFE\"\n")); // bytes that are not UTF-8 inside a string
	CHECK(parses("local id = FourCC('hfoo')\nlocal next = 'hfoo' + 1\n"));
	CHECK(parses("\xEF\xBB\xBF" "function main() end\n"));
	CHECK(parses("#!/usr/bin/lua\nfunction main() end\n"));
	CHECK(parses("--[[ block\ncomment ]] local a = 1 -- line comment\n--[==[ another ]==]\n"));
	CHECK(parses("local t = { [1] = 'a', b = \"c\"; 3, }\nlocal n = 0x1p4 + 1e-3 + 0xFF\n"));
	CHECK(parses("function TRIGSTR_test() return \"TRIGSTR_001\" end\n"));
}

TEST_CASE("check_lua_syntax refuses invalid Lua and reports the line") {
	{
		const auto error = check_lua_syntax("local p = \"war3mapImported\\s.mdx\"\n");
		REQUIRE(error.has_value());
		CHECK(error->line == 1);
		CHECK(contains(error->message, "invalid escape sequence"));
	}
	{
		const auto error = check_lua_syntax("local a = 1\n\nif a != 2 then end\n");
		REQUIRE(error.has_value());
		CHECK(error->line == 3);
		CHECK_FALSE(error->at_end);
	}
	{
		std::string locals = "function f()\n";
		for (int i = 0; i < 201; i++) {
			locals += std::format("local v{} = {}\n", i, i);
		}
		locals += "end\n";
		const auto error = check_lua_syntax(locals);
		REQUIRE(error.has_value());
		CHECK(contains(error->message, "too many local variables"));
	}
	{
		constexpr char with_nul[] = "local a = 1\nlocal b = 2 \0 3\n";
		const auto error = check_lua_syntax(std::string_view(with_nul, sizeof(with_nul) - 1));
		REQUIRE(error.has_value());
		CHECK(error->line == 2);
	}
	{
		const auto error = check_lua_syntax("function a()\nlocal x = 1\n\n");
		REQUIRE(error.has_value());
		CHECK(error->line == 4);
		CHECK(error->opened_line == 1);
		CHECK(error->at_end);
		CHECK(contains(error->message, "'end' expected"));
	}
	{
		// A binary chunk is refused in text mode; that message has no line.
		const auto error = check_lua_syntax("\x1bLua\x53");
		REQUIRE(error.has_value());
		CHECK(error->line == 0);
	}
}

TEST_CASE("check_lua_syntax counts lines as Lua does, also after a BOM or a leading # line") {
	// \n, \r, \r\n and \n\r are one line each; \n\n and \r\r are two.
	CHECK(error_line("a = 1\nb = 2\r\nc = 3\rd = 4\n\re = 5\n\nf = = 6\n") == 7);
	CHECK(error_line("a = 1\r\rf = = 6\n") == 3);
	CHECK(error_line("\xEF\xBB\xBF" "a = 1\nf = = 6\n") == 2);
	CHECK(error_line("# first line\na = 1\nf = = 6\n") == 3);
	CHECK(error_line("# first line\r\na = 1\r\nf = = 6\r\n") == 3);
	CHECK(check_lua_syntax("# only a comment line") == std::nullopt);

	CHECK(lua_line_of_offset("a\r\nb\n\rc\rd", 0) == 1);
	CHECK(lua_line_of_offset("a\r\nb\n\rc\rd", 3) == 2);
	CHECK(lua_line_of_offset("a\r\nb\n\rc\rd", 6) == 3);
	CHECK(lua_line_of_offset("a\r\nb\n\rc\rd", 8) == 4);
}

TEST_CASE("Lua syntax errors name the map header, the trigger or HiveWE-generated code") {
	// Offsets as generate_map_script records them: the header and each custom-text trigger.
	const std::string header = "function Helper()\n  return 1\nend";
	const std::string trigger = "function Trig_Foo_Actions()\n  if x != 1 then end\nend";
	const std::string script = "gg_trg_Foo = nil\n" + header + "\n" + "-- generated\n" + trigger + "\nfunction main()\nend\n";
	const std::size_t header_begin = script.find(header);
	const std::size_t trigger_begin = script.find(trigger);
	const std::vector<ScriptRegion> regions = lines_of_spans(
		script,
		{ { header_begin, header_begin + header.size(), "Map header" },
		  { trigger_begin, trigger_begin + trigger.size(), "Trigger Foo" },
		  { 5, 5, "empty, left out" } }
	);
	REQUIRE(regions.size() == 2);
	CHECK(regions[0].first_line == 2);
	CHECK(regions[0].last_line == 4);
	CHECK(regions[1].first_line == 6);
	CHECK(regions[1].last_line == 8);
	CHECK(region_of_line(regions, 1) == "HiveWE-generated code");
	CHECK(region_of_line(regions, 3) == "Map header");
	CHECK(region_of_line(regions, 5) == "HiveWE-generated code");
	CHECK(region_of_line(regions, 7) == "Trigger Foo");
	CHECK(region_of_line(regions, 9) == "HiveWE-generated code");

	const auto error = check_lua_syntax(script);
	REQUIRE(error.has_value());
	const std::string text = describe_lua_syntax_error(*error, regions);
	CHECK(text.starts_with(lua_syntax_error_prefix));
	CHECK(contains(text, "at line 7 (Trigger Foo): "));

	// An unclosed function in the header is only noticed at the end of the script: the message names the header line it opened at.
	const std::string unclosed = "gg_trg_Foo = nil\nfunction Helper()\n  local y = 1\n\nfunction main()\nend\n";
	const std::vector<ScriptRegion> unclosed_regions = { { 2, 3, "Map header" } };
	const auto eof_error = check_lua_syntax(unclosed);
	REQUIRE(eof_error.has_value());
	CHECK(eof_error->at_end);
	CHECK(eof_error->opened_line == 2);
	const std::string eof_text = describe_lua_syntax_error(*eof_error, unclosed_regions);
	CHECK(contains(eof_text, "(HiveWE-generated code)"));
	CHECK(contains(eof_text, "It refers to line 2 (Map header); the mistake may be anywhere from there to line 7."));

	// An unfinished long string names the line it starts at.
	const auto long_string = check_lua_syntax("a = 1\nb = [[ open\n\n");
	REQUIRE(long_string.has_value());
	CHECK(long_string->opened_line == 2);
	CHECK(long_string->at_end);

	// Without region names (a script HiveWE did not generate).
	CHECK(describe_lua_syntax_error(*error, {}, "It has a Lua syntax error", false).starts_with("It has a Lua syntax error at line 7: "));
}

TEST_CASE("check_lua_syntax runs on several threads at once") {
	std::vector<std::thread> threads;
	std::atomic<int> right = 0;
	for (int t = 0; t < 8; t++) {
		threads.emplace_back([&right, t] {
			for (int i = 0; i < 50; i++) {
				const bool broken = (i + t) % 2 == 0;
				const auto error = check_lua_syntax(broken ? "function f(\n" : "function f() end\n");
				right += error.has_value() == broken ? 1 : 0;
			}
		});
	}
	for (std::thread& thread : threads) {
		thread.join();
	}
	CHECK(right == 400);
}

TEST_CASE("Map Protector refuses a pack whose rewrite broke a Lua script that parsed") {
	// The trigger string holds "]]": inlined into a quoted literal inside a long string, it closes the long string early.
	const fs::path root = make_scratch_dir("protector_broke_script");
	const fs::path temp_dir = root / "input";
	fs::create_directories(temp_dir);
	write_all(temp_dir / "war3map.lua", "local s = [[ \"TRIGSTR_001\" ]]\nfunction main() end\n");
	write_all(temp_dir / "war3map.wts", "STRING 1\r\n{\r\na]]b\r\n}\r\n");
	REQUIRE_FALSE(check_lua_syntax("local s = [[ \"TRIGSTR_001\" ]]\nfunction main() end\n").has_value());

	ProtectionOptions options;
	options.remove_gui_triggers = false;
	options.strip_trigger_strings = true;
	options.inject_junk_files = false;
	options.obfuscate_asset_paths = false;
	// An earlier export at the output path must survive the refusal byte for byte.
	const fs::path output = root / "output" / "protected.w3x";
	fs::create_directories(output.parent_path());
	const std::string old_export = std::string("MPQ\x1a OLD EXPORT\0\x01\x02", 18);
	write_all(output, old_export);
	const PackResult result = run_async_pack(temp_dir, output, options);
	CHECK_FALSE(result.success);
	CHECK(contains(result.error, "broke 'war3map.lua'"));
	CHECK(contains(result.error, "line 1"));
	std::ifstream in(output, std::ios::binary);
	CHECK(std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()) == old_export);
}

TEST_CASE("Map Protector packs a Lua script that was already broken, with a warning") {
	const fs::path root = make_scratch_dir("protector_already_broken");
	const fs::path temp_dir = root / "input";
	fs::create_directories(temp_dir);
	write_all(temp_dir / "war3map.lua", "function main(\nend\n");

	ProtectionOptions options;
	options.remove_gui_triggers = false;
	options.strip_trigger_strings = false;
	options.inject_junk_files = false;
	options.obfuscate_asset_paths = false;
	const fs::path output = root / "output" / "protected.w3x";
	const PackResult result = run_async_pack(temp_dir, output, options);
	CHECK(result.success);
	CHECK(contains(result.warning, "'war3map.lua' already had a syntax error"));
	CHECK(fs::exists(output));
}

TEST_CASE("judge_rewritten_lua_scripts: unchanged good scripts pass without a warning") {
	const LuaSyntaxError error { 3, "unexpected symbol near '!'" };
	CHECK(judge_rewritten_lua_scripts({ { "war3map.lua", std::nullopt } }, { { "war3map.lua", std::nullopt } }).success);
	CHECK(judge_rewritten_lua_scripts({ { "war3map.lua", std::nullopt } }, { { "war3map.lua", std::nullopt } }).warning.empty());
	CHECK_FALSE(judge_rewritten_lua_scripts({ { "war3map.lua", std::nullopt } }, { { "war3map.lua", error } }).success);
	// A script that appears only after the rewrites and does not parse is refused too.
	CHECK_FALSE(judge_rewritten_lua_scripts({}, { { "scripts\\war3map.lua", error } }).success);
	const PackResult kept = judge_rewritten_lua_scripts({ { "war3map.lua", error } }, { { "war3map.lua", error } });
	CHECK(kept.success);
	CHECK(contains(kept.warning, "line 3"));
}

// Sweep: set HIVEWE_LUA_SWEEP_ROOT to a folder; every war3map.lua below it (each distinct file once) must parse. Passes with 0 files
// when the variable is unset, so check the printed count. HIVEWE_LUA_SWEEP_MAX_FAILURES allows known broken fixtures.
TEST_CASE("Lua sweep: every war3map.lua below HIVEWE_LUA_SWEEP_ROOT parses") {
#pragma warning(suppress : 4996) // getenv: read once on a single-threaded test.
	const char* root = std::getenv("HIVEWE_LUA_SWEEP_ROOT");
	if (!root || !*root) {
		MESSAGE("HIVEWE_LUA_SWEEP_ROOT is not set: 0 war3map.lua files checked");
		return;
	}
#pragma warning(suppress : 4996)
	const char* allowed_text = std::getenv("HIVEWE_LUA_SWEEP_MAX_FAILURES");
	const int allowed = allowed_text ? std::atoi(allowed_text) : 0;

	std::unordered_set<std::string> seen;
	int checked = 0;
	int failed = 0;
	std::error_code ec;
	for (auto it = fs::recursive_directory_iterator(fs::path(root), fs::directory_options::skip_permission_denied, ec);
		 !ec && it != fs::recursive_directory_iterator();
		 it.increment(ec)) {
		std::error_code type_ec;
		if (!it->is_regular_file(type_ec)) {
			continue;
		}
		std::wstring name = it->path().filename().wstring();
		std::ranges::transform(name, name.begin(), [](const wchar_t c) { return std::towlower(c); });
		if (name != L"war3map.lua") {
			continue;
		}
		std::ifstream in(it->path(), std::ios::binary);
		std::string text { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
		if (!seen.insert(text).second) {
			continue;
		}
		checked++;
		if (const auto error = check_lua_syntax(text)) {
			failed++;
			const std::u8string path = it->path().u8string();
			const std::string shown(path.begin(), path.end());
			MESSAGE("does not parse: ", shown, " line ", error->line, ": ", error->message);
		}
	}
	MESSAGE("Lua sweep: ", checked, " distinct war3map.lua files checked, ", checked - failed, " parse, ", failed, " do not");
	std::println("Lua sweep: {} distinct war3map.lua files checked, {} parse, {} do not", checked, checked - failed, failed);
	CHECK(failed <= allowed);
}
