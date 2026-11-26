module;

#include <lua53/lua.hpp>

export module LuaSyntax;

import std;

/// Syntax check of war3map.lua through the Lua 5.3 parser (overlay-ports/lua53; the game embeds Lua 5.3.4, same grammar). Qt-free and
/// thread-safe: every check uses its own lua_State, and the chunk is only compiled, never run.

/// Every syntax error message install_map_script returns for a Lua map starts with this, so the save dialog can tell it from a write failure.
export constexpr std::string_view lua_syntax_error_prefix = "The generated war3map.lua has a Lua syntax error";

export struct LuaSyntaxError {
	/// 1-based line Lua reports, or 0 when the message has none (for example a binary chunk).
	int line = 0;
	/// Lua's message without the "war3map.lua:<line>: " prefix.
	std::string message;
	/// The line of the block or long string the error points back to: "(to close 'function' at line M)" or "(starting at line M)". 0 if none.
	int opened_line = 0;
	/// The parser reached the end of the script ("near <eof>"), so the mistake may be anywhere before the reported line.
	bool at_end = false;
};

/// A named byte range [begin, end) of a script.
export struct ScriptSpan {
	std::size_t begin = 0;
	std::size_t end = 0;
	std::string name;
};

/// A named range of 1-based lines, both ends inclusive.
export struct ScriptRegion {
	int first_line = 0;
	int last_line = 0;
	std::string name;
};

/// The name of every line no region covers.
export constexpr std::string_view generated_code_region = "HiveWE-generated code";

/// Offsets where each line starts, counted as the Lua lexer counts: "\n", "\r", "\r\n" and "\n\r" each end one line.
std::vector<std::size_t> lua_line_starts(const std::string_view text) {
	std::vector<std::size_t> starts { 0 };
	for (std::size_t i = 0; i < text.size(); i++) {
		const char c = text[i];
		if (c != '\n' && c != '\r') {
			continue;
		}
		if (i + 1 < text.size() && (text[i + 1] == '\n' || text[i + 1] == '\r') && text[i + 1] != c) {
			i++;
		}
		starts.push_back(i + 1);
	}
	return starts;
}

int line_at(const std::vector<std::size_t>& starts, const std::size_t offset) {
	return static_cast<int>(std::ranges::upper_bound(starts, offset) - starts.begin());
}

/// 1-based line of the byte at `offset`, counted as Lua counts lines.
export int lua_line_of_offset(const std::string_view text, const std::size_t offset) {
	return line_at(lua_line_starts(text), offset);
}

/// The lines each non-empty span covers in `text`.
export std::vector<ScriptRegion> lines_of_spans(const std::string_view text, const std::vector<ScriptSpan>& spans) {
	const std::vector<std::size_t> starts = lua_line_starts(text);
	std::vector<ScriptRegion> regions;
	for (const ScriptSpan& span : spans) {
		if (span.end <= span.begin || span.end > text.size()) {
			continue;
		}
		regions.push_back({ line_at(starts, span.begin), line_at(starts, span.end - 1), span.name });
	}
	return regions;
}

/// The region holding `line`, or generated_code_region. Regions do not overlap; the first match wins.
export std::string region_of_line(const std::vector<ScriptRegion>& regions, const int line) {
	for (const ScriptRegion& region : regions) {
		if (line >= region.first_line && line <= region.last_line) {
			return region.name;
		}
	}
	return std::string(generated_code_region);
}

/// The number after `marker` in `text`, or 0.
int number_after(const std::string_view text, const std::string_view marker) {
	const std::size_t at = text.find(marker);
	if (at == std::string_view::npos) {
		return 0;
	}
	int value = 0;
	const char* first = text.data() + at + marker.size();
	const auto [end, error] = std::from_chars(first, text.data() + text.size(), value);
	return error == std::errc() && end != first ? value : 0;
}

/// Splits "war3map.lua:<line>: <message>" as luaL_loadbufferx reports it for the chunk name "=war3map.lua".
LuaSyntaxError parse_lua_error(const std::string_view raw) {
	LuaSyntaxError error;
	error.message = std::string(raw);
	constexpr std::string_view chunk = "war3map.lua:";
	if (raw.starts_with(chunk)) {
		int line = 0;
		const char* first = raw.data() + chunk.size();
		const auto [end, code] = std::from_chars(first, raw.data() + raw.size(), line);
		const std::string_view rest(end, raw.data() + raw.size());
		if (code == std::errc() && end != first && rest.starts_with(": ")) {
			error.line = line;
			error.message = std::string(rest.substr(2));
		}
	}
	if (error.message.contains("(to close ")) {
		error.opened_line = number_after(error.message, " at line ");
	} else {
		error.opened_line = number_after(error.message, "(starting at line ");
	}
	error.at_end = error.message.ends_with("near <eof>");
	return error;
}

/// Parses `source` as a Lua 5.3 text chunk without running it. Returns the first syntax error, or nothing when it parses.
/// A leading UTF-8 BOM and a first line starting with '#' are skipped as luaL_loadfile does, keeping line numbers.
export std::optional<LuaSyntaxError> check_lua_syntax(std::string_view source) {
	if (source.starts_with("\xEF\xBB\xBF")) {
		source.remove_prefix(3);
	}
	if (source.starts_with('#')) {
		// The line break stays, so the next line is still line 2.
		const std::size_t line_end = source.find_first_of("\r\n");
		source = line_end == std::string_view::npos ? std::string_view() : source.substr(line_end);
	}

	const std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(), &lua_close);
	if (!state) {
		return LuaSyntaxError { 0, "not enough memory to start the Lua parser" };
	}
	const int status = luaL_loadbufferx(state.get(), source.data(), source.size(), "=war3map.lua", "t");
	if (status == LUA_OK) {
		return std::nullopt; // The compiled chunk is discarded with the state; it is never called.
	}
	const char* message = lua_tostring(state.get(), -1);
	return parse_lua_error(message ? std::string_view(message) : std::string_view("unknown Lua error"));
}

/// For the user: `lead`, the line with its region and Lua's message, plus where to look when the parser only noticed at a later line.
/// Without `name_regions` (a script HiveWE did not generate) lines get no region name.
export std::string describe_lua_syntax_error(
	const LuaSyntaxError& error,
	const std::vector<ScriptRegion>& regions,
	const std::string_view lead = lua_syntax_error_prefix,
	const bool name_regions = true
) {
	const auto region = [&](const int line) { return name_regions ? " (" + region_of_line(regions, line) + ")" : std::string(); };
	std::string text(lead);
	if (error.line > 0) {
		text += std::format(" at line {}{}: {}.", error.line, region(error.line), error.message);
	} else {
		text += std::format(": {}.", error.message);
	}
	if (error.opened_line > 0 && error.opened_line != error.line) {
		text += std::format(
			" It refers to line {}{}; the mistake may be anywhere from there to line {}.", error.opened_line, region(error.opened_line), error.line
		);
	} else if (error.at_end) {
		text += " The script ended while something was still open, so the cause may be earlier, for example a missing 'end' or an "
				"unclosed bracket or string.";
	}
	return text;
}
