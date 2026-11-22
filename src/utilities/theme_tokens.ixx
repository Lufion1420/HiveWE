export module ThemeTokens;

import std;

/// Design tokens and the QSS template substitution of the "Flat" themes (data/themes/Flat.tokens and Flat.qss.in).
/// Std only, so the parsing and substitution are unit tested without Qt.
export namespace theme_tokens {
using TokenMap = std::map<std::string, std::string, std::less<>>;

/// Parses the token file text: `[section]` lines, `name = value` lines, `#` or `;` at the start of a line is a comment.
/// The result holds every token of the "shared" section overridden by the tokens of `theme_section`.
/// An error for a missing theme section, a line outside a section, or a line that is not `name = value`.
std::expected<TokenMap, std::string> parse_tokens(std::string_view text, std::string_view theme_section);

/// Replaces every `@{name}` in the template with its token value.
/// An error names the first unknown token or the first unterminated `@{`, so a misspelled key cannot pass silently.
std::expected<std::string, std::string> substitute_tokens(std::string_view tmpl, const TokenMap& tokens);

struct StartupTheme {
	/// The theme that is active afterwards; empty when even the fallback failed (the application stays unstyled).
	std::string applied;
	/// Why the requested theme failed; empty when it applied.
	std::string error;
};

/// Startup rule: try `requested`; if `apply` (returns an error text, empty on success) fails, try "Dark". Never returns an error to the caller
/// as a reason to quit: a broken theme file must not stop HiveWE from starting.
StartupTheme apply_with_fallback(const std::string& requested, const std::function<std::string(const std::string&)>& apply);
}

namespace theme_tokens {
StartupTheme apply_with_fallback(const std::string& requested, const std::function<std::string(const std::string&)>& apply) {
	std::string error = apply(requested);
	if (error.empty()) {
		return { requested, {} };
	}

	if (requested != "Dark") {
		const std::string fallback_error = apply("Dark");
		if (fallback_error.empty()) {
			return { "Dark", error };
		}
		error += "; the fallback theme Dark failed too: " + fallback_error;
	}
	return { {}, error };
}

std::string_view trim(std::string_view text) {
	constexpr std::string_view whitespace = " \t\r\n";
	const auto first = text.find_first_not_of(whitespace);
	if (first == std::string_view::npos) {
		return {};
	}
	const auto last = text.find_last_not_of(whitespace);
	return text.substr(first, last - first + 1);
}

std::expected<TokenMap, std::string> parse_tokens(std::string_view text, std::string_view theme_section) {
	std::map<std::string, TokenMap, std::less<>> sections;
	TokenMap* current = nullptr;
	int line_number = 0;

	while (!text.empty()) {
		++line_number;
		const auto newline = text.find('\n');
		const std::string_view line = trim(text.substr(0, newline));
		text = newline == std::string_view::npos ? std::string_view {} : text.substr(newline + 1);

		if (line.empty() || line.front() == '#' || line.front() == ';') {
			continue;
		}

		if (line.front() == '[') {
			if (line.back() != ']') {
				return std::unexpected(std::format("line {}: unterminated section header", line_number));
			}
			current = &sections[std::string(trim(line.substr(1, line.size() - 2)))];
			continue;
		}

		const auto equals = line.find('=');
		if (equals == std::string_view::npos) {
			return std::unexpected(std::format("line {}: expected 'name = value'", line_number));
		}
		if (current == nullptr) {
			return std::unexpected(std::format("line {}: token outside a section", line_number));
		}

		const std::string_view name = trim(line.substr(0, equals));
		if (name.empty()) {
			return std::unexpected(std::format("line {}: empty token name", line_number));
		}
		(*current)[std::string(name)] = std::string(trim(line.substr(equals + 1)));
	}

	const auto theme = sections.find(theme_section);
	if (theme == sections.end()) {
		return std::unexpected(std::format("missing section [{}]", theme_section));
	}

	TokenMap result;
	if (const auto shared = sections.find("shared"); shared != sections.end()) {
		result = shared->second;
	}
	for (const auto& [name, value] : theme->second) {
		result[name] = value;
	}
	return result;
}

std::expected<std::string, std::string> substitute_tokens(std::string_view tmpl, const TokenMap& tokens) {
	std::string result;
	result.reserve(tmpl.size());

	while (!tmpl.empty()) {
		const auto start = tmpl.find("@{");
		if (start == std::string_view::npos) {
			result.append(tmpl);
			break;
		}

		result.append(tmpl.substr(0, start));
		const auto end = tmpl.find('}', start);
		if (end == std::string_view::npos) {
			return std::unexpected("unterminated '@{' in template");
		}

		const std::string_view name = tmpl.substr(start + 2, end - start - 2);
		const auto token = tokens.find(name);
		if (token == tokens.end()) {
			return std::unexpected(std::format("unknown token '{}'", name));
		}
		result.append(token->second);
		tmpl = tmpl.substr(end + 1);
	}

	return result;
}
}
