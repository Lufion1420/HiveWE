// doctest forward-declares std::char_traits<char>; std::string needs the real one before doctest.h when only `import std` follows.
#include <string>
#include <doctest/doctest.h>

import std;
import ThemeTokens;

namespace fs = std::filesystem;
using namespace theme_tokens;

namespace {
std::string read_text(const fs::path& path) {
	std::ifstream file(path, std::ios::binary);
	REQUIRE_MESSAGE(file.good(), "cannot open " << path.string());
	return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}
}

TEST_CASE("parse_tokens merges the shared section with the theme section") {
	const auto tokens = parse_tokens("# comment\n[shared]\nsize = 12\ncolor = #111111\n\n[dark]\ncolor = #222222\n[light]\ncolor = #eeeeee\n", "dark");
	REQUIRE(tokens.has_value());
	CHECK(tokens->at("size") == "12");
	CHECK(tokens->at("color") == "#222222");

	const auto light = parse_tokens("[shared]\ncolor = #111111\n[light]\ncolor = #eeeeee\r\nname = A, \"B\"\r\n", "light");
	REQUIRE(light.has_value());
	CHECK(light->at("color") == "#eeeeee");
	CHECK(light->at("name") == "A, \"B\"");
}

TEST_CASE("parse_tokens rejects malformed token files") {
	CHECK_FALSE(parse_tokens("[shared]\na = 1\n", "dark").has_value());
	CHECK_FALSE(parse_tokens("a = 1\n[dark]\n", "dark").has_value());
	CHECK_FALSE(parse_tokens("[dark]\nnot a token\n", "dark").has_value());
	CHECK_FALSE(parse_tokens("[dark\na = 1\n", "dark").has_value());
	CHECK_FALSE(parse_tokens("[dark]\n = 1\n", "dark").has_value());
}

TEST_CASE("substitute_tokens replaces every token and reports unknown or unterminated ones") {
	const TokenMap tokens = { { "color-text", "#dfe1e5" }, { "radius", "3" } };

	const auto result = substitute_tokens("A { color: @{color-text}; border-radius: @{radius}px; } B { color: @{color-text}; }", tokens);
	REQUIRE(result.has_value());
	CHECK(*result == "A { color: #dfe1e5; border-radius: 3px; } B { color: #dfe1e5; }");

	const auto unknown = substitute_tokens("A { color: @{color-txet}; }", tokens);
	REQUIRE_FALSE(unknown.has_value());
	CHECK(unknown.error().contains("color-txet"));

	CHECK_FALSE(substitute_tokens("A { color: @{color-text; }", tokens).has_value());
	CHECK(substitute_tokens("plain text, an @ alone and { braces }", tokens).value() == "plain text, an @ alone and { braces }");
}

TEST_CASE("the Flat theme files build a complete style sheet for both themes") {
	const fs::path themes = THEMES_DIR;
	const std::string token_text = read_text(themes / "Flat.tokens");
	const std::string template_text = read_text(themes / "Flat.qss.in");

	for (const std::string_view section : { "dark", "light" }) {
		CAPTURE(section);
		auto tokens = parse_tokens(token_text, section);
		REQUIRE(tokens.has_value());
		// Added at run time by src/main_window/theme.cpp.
		(*tokens)["asset-dir"] = "C:/cache/theme-assets";

		const auto stylesheet = substitute_tokens(template_text, *tokens);
		REQUIRE_MESSAGE(stylesheet.has_value(), stylesheet.error());
		CHECK_FALSE(stylesheet->contains("@{"));
		CHECK(stylesheet->contains("#3574f0"));
		CHECK(stylesheet->contains("C:/cache/theme-assets/arrow-down.png"));

		// Every colour token that theme.cpp builds the palette from.
		for (const char* name : { "color-window", "color-base", "color-alt-base", "color-text", "color-text-muted", "color-text-disabled",
								  "color-accent-text", "color-selection", "color-selection-text", "color-border", "color-border-strong",
								  "color-hover", "color-menu-bg", "color-button-top", "color-error" }) {
			CAPTURE(name);
			CHECK(tokens->contains(name));
		}
	}
}

TEST_CASE("apply_with_fallback falls back to Dark and then to unstyled") {
	std::vector<std::string> tried;
	const auto failing = [&](const std::set<std::string>& broken) {
		return [&tried, broken](const std::string& name) -> std::string {
			tried.push_back(name);
			return broken.contains(name) ? "broken " + name : std::string();
		};
	};

	const auto ok = apply_with_fallback("Flat Dark", failing({}));
	CHECK(ok.applied == "Flat Dark");
	CHECK(ok.error.empty());

	tried.clear();
	const auto fell_back = apply_with_fallback("Flat Dark", failing({ "Flat Dark" }));
	CHECK(fell_back.applied == "Dark");
	CHECK(fell_back.error == "broken Flat Dark");
	CHECK(tried == std::vector<std::string> { "Flat Dark", "Dark" });

	tried.clear();
	const auto unstyled = apply_with_fallback("Flat Light", failing({ "Flat Light", "Dark" }));
	CHECK(unstyled.applied.empty());
	CHECK(unstyled.error.find("broken Flat Light") != std::string::npos);
	CHECK(unstyled.error.find("broken Dark") != std::string::npos);

	tried.clear();
	const auto dark_broken = apply_with_fallback("Dark", failing({ "Dark" }));
	CHECK(dark_broken.applied.empty());
	CHECK(tried.size() == 1);
}
