module;

#include <cstdlib>
#include <filesystem>

export module AssetObfuscation;

import std;
import SLK;
import ModificationTables;
import StockObjectData;
import Hierarchy;
import MapInfo;
import MDX;
import BinaryWriter;
import Utilities;
import "absl/strings/str_split.h";
import "absl/strings/str_join.h";

namespace fs = std::filesystem;

/// Normalizes a path string for case/separator-insensitive comparison against other paths (never
/// for display or for the string actually written back into map data - see RenameCandidate below).
/// Starts from Map::get_file_usage()'s match_key lambda (src/base/map/unused_files.cpp) - WC3 paths
/// are case-insensitive, and a model may be referenced as .mdl in one place and imported as .mdx in
/// another (or vice versa), so both the case and that specific extension pair are unified - but adds
/// normalizations that lambda doesn't have: WC3 (Reforged specifically) resolves a texture path
/// against .blp/.tga/.png somewhat interchangeably (the same class of fallback as .mdl/.mdx for
/// models). Found via two separate real-map cases: a war3mapSkin.txt CustomSkin override that named a
/// texture ".tga" when the actual imported file was ".blp", and - far more widespread - a custom UI
/// built with Reforged UI Designer (RUI), whose generated BlzFrameSetTexture() calls reference every
/// frame's texture by its *design-time* ".png" filename (114 call sites in one real map's compiled
/// script) even though the map only ever ships the compiled ".blp". Either way, a rename candidate
/// built from the real file's actual extension silently failed to match the reference (both for
/// rewriting it and for the dangling-reference safety net, since both go through this same function),
/// leaving the frame/override pointing at a name nothing on disk had anymore - the exact cause of
/// widespread missing-texture green boxes across a from-RUI custom UI. Not folded back into
/// unused_files.cpp's copy - that's a pre-existing, separate gap outside this session's scope.
export std::string asset_match_key(std::string path) {
	std::ranges::transform(path, path.begin(), [](const unsigned char c) {
		return c == '\\' ? '/' : static_cast<char>(std::tolower(c));
	});
	if (path.ends_with(".mdl")) {
		path = path.substr(0, path.size() - 4) + ".mdx";
	} else if (path.ends_with(".tga") || path.ends_with(".png")) {
		path = path.substr(0, path.size() - 4) + ".blp";
	}
	return path;
}

/// Files placed under these stock game-asset roots override game content by path (terrain tiles,
/// cliffs, team colours, ...) - the override only works because the path matches a specific stock
/// name, so renaming one would silently break the override rather than just an ordinary reference.
/// Mirrors Map::get_file_usage()'s is_stock_override_path lambda.
bool is_stock_override_path(const std::string& lowercase_forward_slash_path) {
	static constexpr std::array<std::string_view, 2> roots = { "terrainart/", "replaceabletextures/" };
	return std::ranges::any_of(roots, [&](std::string_view root) { return lowercase_forward_slash_path.starts_with(root); });
}

/// True for a filename that's OS-generated metadata, not WC3 map data - the Warcraft III engine
/// never reads these regardless of what they contain, so they're excluded both from renaming
/// (pointless - the game doesn't care what they're called) and from the dangling-reference
/// verification scan (a stale reference inside one doesn't matter to gameplay). Found via a real
/// case: a map whose source folder had once had its Windows folder icon customized picked up a
/// "desktop.ini" (Explorer's folder-customization file) referencing the icon's path, which the
/// verification scan flagged as an unresolved reference and correctly-by-its-own-rules aborted the
/// export over - correct behavior for anything WC3 actually reads, wrong call for a file the engine
/// never opens at all.
bool is_os_metadata_filename(const std::string& filename) {
	std::string lowercase = filename;
	std::ranges::transform(lowercase, lowercase.begin(), [](const unsigned char c) { return std::tolower(c); });
	return lowercase == "desktop.ini";
}

bool is_filename_continuation_char(const char c) {
	return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
}

/// True if `needle` (a candidate's match_key, already lowercased) appears in `content` as a
/// standalone path reference, not merely as a substring embedded inside a longer, unrelated
/// filename. Found via a real case: a candidate literally named "ground.blp" byte-matched inside
/// "blank-background.blp" in a real map's war3mapSkin.txt (back-GROUND.blp) - a coincidental
/// substring collision, not an actual reference to the renamed file, that a plain `.find() != npos`
/// check can't tell apart from a real one. Requires the character immediately before and after each
/// occurrence, if any, to not itself be a filename-continuation character (alphanumeric/'_'/'-'); a
/// path separator, quote, '=', whitespace, or the start/end of the content all count as valid
/// boundaries. Keeps scanning past a rejected occurrence rather than giving up after the first one,
/// in case a later occurrence of the same needle is a genuine reference.
bool contains_path_reference(const std::string& content, const std::string& needle) {
	if (needle.empty()) {
		return false;
	}
	size_t pos = 0;
	while ((pos = content.find(needle, pos)) != std::string::npos) {
		const bool leading_ok = pos == 0 || !is_filename_continuation_char(content[pos - 1]);
		const size_t after = pos + needle.size();
		const bool trailing_ok = after >= content.size() || !is_filename_continuation_char(content[after]);
		if (leading_ok && trailing_ok) {
			return true;
		}
		pos += 1;
	}
	return false;
}

/// Escapes text for embedding as the body of a JASS or Lua double-quoted string literal. Both
/// languages accept the same core escapes (\\, \", \n) for this purpose. Control bytes other
/// than newline are dropped rather than risking an escape sequence the game's script parser
/// doesn't accept - trigger string text is always printable text in practice (WC3 itself uses
/// the literal 2-character sequence "|n", not a real newline byte, for tooltip line breaks).
export std::string escape_script_string(const std::string& text) {
	std::string result;
	result.reserve(text.size());
	for (const char c : text) {
		switch (c) {
			case '\\':
				result += "\\\\";
				break;
			case '"':
				result += "\\\"";
				break;
			case '\n':
				result += "\\n";
				break;
			case '\r':
				break; // dropped: a lone \n is already a valid escaped line break
			default:
				if (static_cast<unsigned char>(c) >= 0x20) {
					result += c;
				}
				break;
		}
	}
	return result;
}

/// Inverse of escape_script_string(): turns a string literal's raw, still-escaped source content
/// back into the plain text it represents (\\ -> \, \" -> ", \n -> a real newline byte). Needed
/// because asset paths inside a literal are written pre-escaped in the script (e.g. a single
/// backslash path separator appears as the two-character sequence \\), and asset_match_key() must
/// compare against the real path text, not its escaped-for-source-code form, or every backslash
/// would be mis-read as two separate characters and never match a candidate. An unrecognized escape
/// sequence is left as-is (the backslash is kept, nothing is consumed) rather than guessed at.
export std::string unescape_script_string(const std::string& raw) {
	std::string result;
	result.reserve(raw.size());
	for (size_t i = 0; i < raw.size(); ++i) {
		if (raw[i] == '\\' && i + 1 < raw.size()) {
			switch (raw[i + 1]) {
				case '\\':
					result += '\\';
					++i;
					continue;
				case '"':
					result += '"';
					++i;
					continue;
				case 'n':
					result += '\n';
					++i;
					continue;
				default:
					break;
			}
		}
		result += raw[i];
	}
	return result;
}

/// One quoted string literal found by find_string_literals(), exported alongside it so both
/// protection_pipeline.ixx's TRIGSTR/asset-literal rewrite passes and this file's own
/// verify_no_dangling_text_references() safety net share one JASS/Lua-aware scan instead of each
/// maintaining its own.
export struct StringLiteralSpan {
	size_t start; // index of the opening quote
	size_t end; // index one past the closing quote
	std::string raw_content; // between the quotes, still escaped as it appears in source
};

/// Scans `text` (a JASS or Lua source file's full contents) for quoted string literals, skipping
/// both languages' line/block comment styles ("//", "/* */", "--", "--[[ ]]") so a quote character
/// inside a comment can't desynchronize the scan for everything after it. Respects \\ and the
/// matching quote character so an escaped quote doesn't end a literal early.
///
/// allow_single_quote_strings must be true for Lua and false for JASS: Lua accepts '...' and "..."
/// interchangeably for strings (a real map was found using '...' for several AddSpecialEffect-style
/// calls, which this scanner originally missed entirely since it only recognized "..."), but in JASS
/// '...' is not a string at all - it's a 4-character rawcode literal (e.g. 'hfoo') that compiles to
/// an integer constant. Treating a JASS rawcode as a string here would misclassify it, and rewriting
/// it as one would corrupt the script.
export std::vector<StringLiteralSpan> find_string_literals(const std::string& text, const bool allow_single_quote_strings) {
	std::vector<StringLiteralSpan> spans;
	size_t i = 0;

	const auto scan_literal = [&](const char quote) {
		const size_t start = i;
		std::string content;
		++i;
		bool terminated = false;
		while (i < text.size() && text[i] != '\n') {
			if (text[i] == quote) {
				terminated = true;
				++i;
				break;
			}
			if (text[i] == '\\' && i + 1 < text.size()) {
				content += text[i];
				content += text[i + 1];
				i += 2;
			} else {
				content += text[i];
				++i;
			}
		}
		if (terminated) {
			spans.push_back({ start, i, content });
		}
		// Unterminated literal (shouldn't happen in valid generated script): leave it out of the
		// results rather than guessing where it ends.
	};

	while (i < text.size()) {
		if (text.compare(i, 2, "//") == 0) {
			while (i < text.size() && text[i] != '\n') {
				++i;
			}
		} else if (text.compare(i, 2, "/*") == 0) {
			const size_t close = text.find("*/", i + 2);
			i = (close == std::string::npos) ? text.size() : close + 2;
		} else if (text.compare(i, 4, "--[[") == 0) {
			const size_t close = text.find("]]", i + 4);
			i = (close == std::string::npos) ? text.size() : close + 2;
		} else if (text.compare(i, 2, "--") == 0) {
			while (i < text.size() && text[i] != '\n') {
				++i;
			}
		} else if (text[i] == '"') {
			scan_literal('"');
		} else if (allow_single_quote_strings && text[i] == '\'') {
			scan_literal('\'');
		} else {
			++i;
		}
	}
	return spans;
}

/// A single loose file under temp_dir that Asset Path Obfuscation will rename. new_relative_path is
/// always a flat, single-segment name directly under the archive root (e.g. "a3f9c1e2.mdx") -
/// nothing about WC3's asset loading depends on folder structure, only on every reference to a file
/// agreeing on its path, so flattening is free obfuscation with no extra correctness burden: later
/// phases rewrite every reference (including paths stored inside another model's own MDX chunks) to
/// this exact same string, not a folder-relative one.
export struct RenameCandidate {
	fs::path original_relative_path;
	fs::path new_relative_path;
	std::string match_key;
};

namespace {
	std::string generate_garbage_name(const fs::path& extension, std::mt19937& rng, const std::unordered_set<std::string>& taken) {
		static constexpr std::string_view hex_digits = "0123456789abcdef";
		std::uniform_int_distribution<int> digit(0, static_cast<int>(hex_digits.size()) - 1);

		std::string lowercase_extension = extension.string();
		std::ranges::transform(lowercase_extension, lowercase_extension.begin(), [](const unsigned char c) { return std::tolower(c); });

		for (;;) {
			std::string name;
			for (int i = 0; i < 8; ++i) {
				name += hex_digits[digit(rng)];
			}
			name += lowercase_extension;
			if (!taken.contains(name)) {
				return name;
			}
		}
	}

	bool starts_with_ci(const std::string& value, const std::string_view prefix) {
		if (value.size() < prefix.size()) {
			return false;
		}
		return std::ranges::equal(value.substr(0, prefix.size()), prefix, [](const unsigned char a, const unsigned char b) {
			return std::tolower(a) == std::tolower(b);
		});
	}
} // namespace

/// Enumerates every loose file under temp_dir that Asset Path Obfuscation is allowed to rename.
///
/// Excludes:
/// - Anything in never_rename_file_names (Imports::blacklist in production - core war3map.*/
///   war3campaign.* filenames the engine looks up by hardcoded name, plus war3mapSkin.txt, which
///   is already in that same blacklist and which HiveWE doesn't parse anywhere so is left untouched
///   rather than risk desyncing a format nothing here understands).
/// - Stock-override paths (TerrainArt/, ReplaceableTextures/), which must keep their exact stock
///   name for the override itself to work.
/// - Anything is_stock_asset() reports as also existing as a genuine stock/base-game asset at the
///   same relative path - a broader safety net beyond the two known override roots. Injected rather
///   than hard-coded to the global Hierarchy singleton so this stays independently testable; the
///   pipeline's production call site passes hierarchy.game_file_exists().
/// - OS-generated metadata files (currently just desktop.ini) - the engine never reads these, so
///   renaming one is pointless regardless of what it contains.
///
/// Sound files are excluded by the caller (not filtered here) since exclusion in v1 is driven by
/// which files are referenced by a Sound object, which this file-system-only enumeration can't know
/// - that cross-reference is resolved once the object-data/w3s reference-discovery phases exist.
///
/// TEMPORARY BISECTION HOOKS (2026-08-07, tracking an unresolved in-game crash on real map data that
/// survives every static/archive-level check tried so far): two env vars, read once, read only here,
/// both no-ops unless set, so production behavior is unchanged by default.
/// - HIVEWE_MP_EXCLUDE_EXTS: comma-separated extensions (no dot, case-insensitive, e.g. "mdx,blp")
///   to exclude from candidacy entirely - those files keep their real name/content untouched, same as
///   a blacklist entry, letting a test narrow down whether renaming one specific asset *kind* is what
///   triggers the crash without needing a full rebuild per hypothesis.
/// - HIVEWE_MP_KEEP_DIRS: if set to any non-empty value, new names keep the file's original relative
///   directory instead of flattening to the archive root - isolates "renamed" from "flattened" as
///   separate variables.
/// Remove both once the crash is root-caused; not meant to be permanent, user-facing behavior.
export std::vector<RenameCandidate> enumerate_rename_candidates(
	const fs::path& temp_dir,
	const std::unordered_set<std::string>& never_rename_file_names,
	const std::function<bool(const fs::path&)>& is_stock_asset
) {
	std::vector<RenameCandidate> candidates;
	std::unordered_set<std::string> taken_names;
	std::mt19937 rng{ std::random_device{}() };

	if (!fs::exists(temp_dir)) {
		return candidates;
	}

	const auto read_env = [](const char* name) -> std::optional<std::string> {
		char* value = nullptr;
		size_t length = 0;
		if (_dupenv_s(&value, &length, name) != 0 || !value) {
			return std::nullopt;
		}
		std::string result(value);
		free(value);
		return result;
	};

	std::unordered_set<std::string> excluded_extensions;
	if (const std::optional<std::string> raw = read_env("HIVEWE_MP_EXCLUDE_EXTS")) {
		for (const auto part : std::views::split(std::string_view(*raw), ',')) {
			std::string ext(part.begin(), part.end());
			std::ranges::transform(ext, ext.begin(), [](const unsigned char c) { return std::tolower(c); });
			if (!ext.empty()) {
				excluded_extensions.insert(std::move(ext));
			}
		}
	}
	const bool keep_dirs = read_env("HIVEWE_MP_KEEP_DIRS").has_value();

	for (const auto& entry : fs::recursive_directory_iterator(temp_dir)) {
		if (!entry.is_regular_file()) {
			continue;
		}

		const fs::path relative_path = entry.path().lexically_relative(temp_dir);
		const std::string file_name = entry.path().filename().string();
		if (never_rename_file_names.contains(file_name) || is_os_metadata_filename(file_name)) {
			continue;
		}

		if (!excluded_extensions.empty()) {
			std::string ext = entry.path().extension().string();
			if (!ext.empty() && ext.front() == '.') {
				ext.erase(ext.begin());
			}
			std::ranges::transform(ext, ext.begin(), [](const unsigned char c) { return std::tolower(c); });
			if (excluded_extensions.contains(ext)) {
				continue;
			}
		}

		const std::string key = asset_match_key(relative_path.string());
		// ReplaceableTextures/CommandButtonsDisabled/ is nested under the otherwise-fixed
		// ReplaceableTextures/ stock-override root (see is_stock_override_path()), but a custom
		// button's disabled-state icon there isn't overriding any stock asset by exact path - it's
		// WC3's fixed lookup location for *any* button's disabled icon (custom or stock), derived
		// from the enabled icon's own filename at runtime. It needs to stay renameable, in lockstep
		// with its "BTN..." pair - see the pairing pass below - not pinned to a fixed name.
		static constexpr std::string_view command_buttons_disabled_root = "replaceabletextures/commandbuttonsdisabled/";
		const bool is_custom_disabled_button_icon = key.starts_with(command_buttons_disabled_root);
		if (is_stock_override_path(key) && !is_custom_disabled_button_icon) {
			continue;
		}
		if (is_stock_asset && is_stock_asset(relative_path)) {
			continue;
		}

		RenameCandidate candidate;
		candidate.original_relative_path = relative_path;
		candidate.match_key = key;
		const fs::path new_name = generate_garbage_name(entry.path().extension(), rng, taken_names);
		candidate.new_relative_path = keep_dirs ? (relative_path.parent_path() / new_name) : new_name;
		taken_names.insert(new_name.string());
		candidates.push_back(std::move(candidate));
	}

	// Re-pair "BTN<name>.ext" command-button icons with their "DISBTN<name>.ext" disabled-state
	// counterpart under ReplaceableTextures/CommandButtonsDisabled/. WC3 never stores a reference to
	// the disabled icon anywhere in the map's own data - no rewrite phase could ever find and fix a
	// reference that doesn't exist - it derives the path itself at runtime, purely from the *enabled*
	// icon's own filename: strip a literal "BTN" prefix, prepend "DISBTN", and look for that name
	// specifically in ReplaceableTextures\CommandButtonsDisabled\, regardless of which folder the
	// enabled icon itself lives in. Found via a real map: 422 BTN icons with 418 matching DISBTN
	// counterparts, every one of which would otherwise get its own independent random name from the
	// loop above and permanently break that derivation - showing as a green box specifically whenever
	// a button is disabled (on cooldown, unaffordable, or - the common case in a hero-select-style
	// custom UI - representing a not-yet-unlocked/empty slot, which is why "most" icons looked broken
	// while a few actively-usable ones didn't).
	{
		std::unordered_map<std::string, size_t> candidate_index_by_lowercase_filename;
		for (size_t i = 0; i < candidates.size(); ++i) {
			std::string lowercase_name = candidates[i].original_relative_path.filename().string();
			std::ranges::transform(lowercase_name, lowercase_name.begin(), [](const unsigned char c) { return std::tolower(c); });
			candidate_index_by_lowercase_filename[lowercase_name] = i;
		}

		for (size_t i = 0; i < candidates.size(); ++i) {
			const std::string original_filename = candidates[i].original_relative_path.filename().string();
			if (!starts_with_ci(original_filename, "BTN")) {
				continue;
			}

			std::string lowercase_disabled_name = "disbtn" + original_filename.substr(3);
			std::ranges::transform(lowercase_disabled_name, lowercase_disabled_name.begin(), [](const unsigned char c) { return std::tolower(c); });

			const auto found = candidate_index_by_lowercase_filename.find(lowercase_disabled_name);
			if (found == candidate_index_by_lowercase_filename.end()) {
				continue;
			}

			// Re-derive the enabled icon's new name with a "BTN" prefix preserved (the loop above
			// only ever generates a flat hex name), and point the disabled counterpart at the matching
			// "DISBTN" name inside the one fixed folder the engine always looks in - independent of
			// wherever the enabled icon's own new name ends up (it's already flattened to the archive
			// root like everything else, since that's freely referenceable via the object-data field
			// this pipeline does rewrite).
			const std::string hex_and_extension = candidates[i].new_relative_path.filename().string();
			candidates[i].new_relative_path = "BTN" + hex_and_extension;
			candidates[found->second].new_relative_path = fs::path("ReplaceableTextures") / "CommandButtonsDisabled" / ("DISBTN" + hex_and_extension);
		}
	}

	return candidates;
}

export struct AssetObfuscationResult {
	bool success = false;
	std::string error;
};

/// Rewrites model/icon/modelList/pathingTexture path fields in a single object-data modification
/// file (e.g. temp_dir / "war3map.w3u" or its war3mapSkin.* counterpart) in place. template_slk/
/// meta_slk must already be the fully-populated stock tables for this category
/// (stock_object_data::load()'s output) - not a bare single-file load - since a custom object's
/// oldid parent, or a field only present via one of the many stock .merge()'d files, needs the
/// complete stock dataset to resolve
/// correctly (see field_to_meta_id()'s doc comment in slk.ixx for why the alias/oldid fallback
/// needs the merged view). Returns changed=true if anything was rewritten (and, in that case, the
/// file has already been re-saved) so the caller only needs to act on failure.
///
/// Exported (not file-local) specifically so it's independently testable against small, hand-built
/// in-memory template/meta SLKs - the same style tests/modification_tables_test.cpp already uses -
/// without needing a real Warcraft III installation's stock data mounted through Hierarchy, which
/// rewrite_object_data_references()'s production stock_object_data::load() call requires.
export struct FileRewriteResult {
	bool success = false;
	std::string error;
	bool changed = false;
};

/// Resolves a rename candidate for an object-data field value, tolerating a missing file extension.
/// Root cause of a real map's ability icon fields (e.g. "art"/"Icon - Normal", read in-game via
/// BlzGetAbilityStringLevelField) staying missing-texture after protection: the Object Editor lets an
/// icon/model field omit its extension entirely (confirmed directly against a real map's LNI-exported
/// object data - most custom "art" values were bare, e.g. "Images/Spells/Foo/BTNBar" for an actual
/// on-disk "BTNBar.blp"), and the client resolves it implicitly - the same convention HiveWE's own
/// Texture loader relies on (texture.ixx's replace_extension probing of .tga/.blp/.dds, which is a
/// no-op-turned-append when the path has no extension to begin with). A rename candidate's match_key
/// always carries the real file's actual extension (built from the file on disk), so an extensionless
/// field value would otherwise never match anything and silently keep pointing at the pre-rename name.
/// The type-classification diagnostic (HIVEWE_MP_TYPE_DEBUG) never caught this because it only logs
/// meta-resolution/type failures, not a correctly-typed field whose value simply fails to match.
const RenameCandidate* find_object_data_candidate(
	const std::string& value,
	const std::string_view type,
	const std::unordered_map<std::string, const RenameCandidate*>& candidates_by_match_key
) {
	const std::string key = asset_match_key(value);
	if (const auto found = candidates_by_match_key.find(key); found != candidates_by_match_key.end()) {
		return found->second;
	}

	static constexpr std::array<std::string_view, 5> known_extensions = { ".blp", ".tga", ".png", ".mdx", ".mdl" };
	if (std::ranges::any_of(known_extensions, [&](std::string_view ext) { return key.ends_with(ext); })) {
		return nullptr;
	}

	const std::string_view default_extension = (type == "model" || type == "modelList") ? ".mdx" : ".blp";
	const auto found = candidates_by_match_key.find(key + std::string(default_extension));
	return found != candidates_by_match_key.end() ? found->second : nullptr;
}

export FileRewriteResult rewrite_object_data_file(
	const fs::path& file_path,
	const std::string_view file_name,
	const slk::SLK& template_slk,
	const slk::SLK& meta_slk,
	const bool optional_ints,
	const bool skin,
	const std::unordered_map<std::string, const RenameCandidate*>& candidates_by_match_key
) {
	if (!fs::exists(file_path)) {
		return { true, "", false };
	}

	auto shadow_map_result = extract_modification_shadow_map_path(file_path, template_slk, meta_slk, optional_ints);
	if (!shadow_map_result) {
		return { false, shadow_map_result.error(), false };
	}
	ModificationShadowMap shadow_map = std::move(shadow_map_result.value());

	// Merged stock+shadow view, needed for field_to_meta_id()'s alias/oldid resolution (ability
	// field-name aliasing) - matches how src/models/table_model.ixx's field_type() resolves the
	// same classification for the live Object Editor UI.
	slk::SLK scratch = shadow_map_to_slk(template_slk, shadow_map);

	bool changed = false;
	for (auto& [object_id, fields] : shadow_map) {
		for (auto& [field_name, value] : fields) {
			if (field_name == "oldid") {
				continue;
			}

			// TEMPORARY (2026-08-08): this type whitelist has already needed extending twice for real
			// maps (modelList, pathingTexture, both below) - real maps still show missing-texture green
			// boxes for some object-editor-set icons after those fixes and after the BTN/DISBTN pairing
			// fix, which fits the same pattern: a field whose real value is a path but that this pipeline
			// never actually rewrites, for one of two possible reasons - either field_to_meta_id() can't
			// resolve the field at all (e.g. a per-level field code like the one
			// BlzGetAbilityStringLevelField(ability, ABILITY_SLF_ICON_NORMAL, level) reads, which may
			// use a different field-name/alias shape than the plain "art" field this pipeline has mostly
			// been exercised against), or it resolves but to a "type" not in the accepted set. Logs
			// either case for any field whose *value* looks path-like (ends in a known asset extension),
			// to find the real cause directly from real data instead of guessing - gated on
			// HIVEWE_MP_TYPE_DEBUG (its value is the report file path), a no-op otherwise. Remove once
			// the gap is found and fixed.
			const auto log_type_debug = [&](const std::string_view reason, const std::string_view type_text) {
				static const std::array<std::string_view, 4> path_like_extensions = { ".blp", ".tga", ".mdx", ".mdl" };
				const std::string lowercase_value = [&] {
					std::string v(value);
					std::ranges::transform(v, v.begin(), [](const unsigned char c) { return std::tolower(c); });
					return v;
				}();
				if (!std::ranges::any_of(path_like_extensions, [&](std::string_view ext) { return lowercase_value.ends_with(ext); })) {
					return;
				}
				char* raw = nullptr;
				size_t raw_len = 0;
				if (_dupenv_s(&raw, &raw_len, "HIVEWE_MP_TYPE_DEBUG") == 0 && raw && *raw) {
					std::ofstream report(raw, std::ios::app);
					report << file_name << ": object=" << object_id << " field=" << field_name << " reason=" << reason << " type=" << type_text << " value=" << value << "\n";
				}
				free(raw);
			};

			const auto meta_id = scratch.field_to_meta_id(meta_slk, field_name, object_id);
			if (!meta_id) {
				log_type_debug("NO_META_ID", "?");
				continue;
			}
			const std::string_view type = meta_slk.data<std::string_view>("type", *meta_id);
			if (type != "model" && type != "icon" && type != "modelList" && type != "pathingTexture") {
				log_type_debug("UNHANDLED_TYPE", type);
				continue;
			}

			if (type == "modelList") {
				// Confirmed via a real map: Ability Buff fields (targetart/specialart) are typed
				// "modelList", not "model", in the stock meta SLK - a genuine third type value this
				// pipeline didn't originally account for, which is why these fields were silently
				// skipped rather than rewritten. Every real value seen so far has been a single path,
				// but the type name implies the field format allows a comma-separated list, so this
				// splits, rewrites whichever segments match a candidate, and rejoins - safe either way
				// (a single-path value is just a "list" of one element).
				std::vector<std::string> segments = absl::StrSplit(value, ',');
				bool any_segment_changed = false;
				for (std::string& segment : segments) {
					if (const RenameCandidate* found = find_object_data_candidate(segment, type, candidates_by_match_key)) {
						segment = found->new_relative_path.string();
						any_segment_changed = true;
					}
				}
				if (any_segment_changed) {
					value = absl::StrJoin(segments, ",");
					changed = true;
				}
				continue;
			}

			const RenameCandidate* found = find_object_data_candidate(value, type, candidates_by_match_key);
			if (!found) {
				continue;
			}
			value = found->new_relative_path.string();
			changed = true;
		}
	}

	if (changed) {
		scratch = shadow_map_to_slk(template_slk, shadow_map); // rebuild: shadow_map above was mutated in place
		save_modification_file(file_name, scratch, meta_slk, optional_ints, skin);
	}

	return { true, "", changed };
}

/// Rewrites every model/icon/modelList/pathingTexture object-data field across all 7 categories (units/items/doodads/
/// destructibles/abilities/upgrades/buffs, plus each category's war3mapSkin.* counterpart) that
/// references a renamed candidate, so a unit's icon, an ability's art, a doodad's model override,
/// etc. all point at the new name. Loads its own independent copy of the stock tables via
/// stock_object_data::load() rather than reusing whatever map is currently open in HiveWE - this is
/// what makes Asset Path Obfuscation work for an external-source map that was never loaded into
/// HiveWE's Map object (see map_protection_plan.md's session notes for why that path matters).
///
/// Redirects hierarchy.map_directory to temp_dir for the duration (same established pattern as
/// protection_pipeline.ixx's sanitize_metadata()/inline_map_info_trigger_strings()), since
/// save_modification_file() writes through hierarchy.map_file_write(). Must be called synchronously
/// before any other step that also redirects hierarchy.map_directory.
export AssetObfuscationResult rewrite_object_data_references(const fs::path& temp_dir, const std::vector<RenameCandidate>& candidates) {
	if (candidates.empty()) {
		return { true, "" };
	}

	std::unordered_map<std::string, const RenameCandidate*> candidates_by_match_key;
	for (const RenameCandidate& candidate : candidates) {
		candidates_by_match_key[candidate.match_key] = &candidate;
	}

	struct Category {
		std::string_view file_name;
		std::string_view skin_file_name;
		const stock_object_data::TableSet* tables;
		bool optional_ints;
	};

	const stock_object_data::Tables stock = stock_object_data::load();
	const std::array<Category, 7> categories = { {
		{ "war3map.w3u", "war3mapSkin.w3u", &stock.units, false },
		{ "war3map.w3t", "war3mapSkin.w3t", &stock.items, false },
		{ "war3map.w3a", "war3mapSkin.w3a", &stock.abilities, true },
		{ "war3map.w3b", "war3mapSkin.w3b", &stock.destructibles, false },
		{ "war3map.w3h", "war3mapSkin.w3h", &stock.buffs, false },
		{ "war3map.w3q", "war3mapSkin.w3q", &stock.upgrades, true },
		{ "war3map.w3d", "war3mapSkin.w3d", &stock.doodads, true },
	} };

	const fs::path original_map_directory = hierarchy.map_directory;
	hierarchy.map_directory = temp_dir;

	AssetObfuscationResult result{ true, "" };
	for (const Category& category : categories) {
		for (const auto& [file_name, skin] : { std::pair{ category.file_name, false }, std::pair{ category.skin_file_name, true } }) {
			const FileRewriteResult step = rewrite_object_data_file(
				temp_dir / file_name, file_name, category.tables->data, category.tables->meta, category.optional_ints, skin, candidates_by_match_key
			);
			if (!step.success) {
				result = { false, std::format("Failed to rewrite asset references in {}: {}", file_name, step.error) };
				break;
			}
		}
		if (!result.success) {
			break;
		}
	}

	hierarchy.map_directory = original_map_directory;
	return result;
}

/// Rewrites war3map.w3i's own literal-path fields (loading_screen_model, prologue_screen_model) if
/// either matches a renamed candidate. Reuses the exact MapInfo load/tileset-byte/save pattern
/// already implemented in protection_pipeline.ixx's sanitize_metadata()/inline_map_info_trigger_strings()
/// - MapInfo::save() needs a tileset byte MapInfo::load() itself discards, so it's read directly
/// from war3map.w3e's header instead (same layout Terrain::load() parses).
export AssetObfuscationResult rewrite_map_info_references(const fs::path& temp_dir, const std::vector<RenameCandidate>& candidates) {
	if (candidates.empty() || !fs::exists(temp_dir / "war3map.w3i")) {
		return { true, "" };
	}

	std::unordered_map<std::string, const RenameCandidate*> candidates_by_match_key;
	for (const RenameCandidate& candidate : candidates) {
		candidates_by_match_key[candidate.match_key] = &candidate;
	}

	const fs::path original_map_directory = hierarchy.map_directory;
	hierarchy.map_directory = temp_dir;

	char tileset = 'L';
	if (auto w3e = hierarchy.map_file_read("war3map.w3e"); w3e && w3e->read_string(4) == "W3E!") {
		w3e->advance(4); // format version
		tileset = w3e->read<char>();
	}

	AssetObfuscationResult result{ true, "" };
	try {
		MapInfo info;
		info.load();

		bool changed = false;
		const auto rewrite_field = [&](std::string& field) {
			const auto found = candidates_by_match_key.find(asset_match_key(field));
			if (found != candidates_by_match_key.end()) {
				field = found->second->new_relative_path.string();
				changed = true;
			}
		};
		rewrite_field(info.loading_screen_model);
		rewrite_field(info.prologue_screen_model);

		if (changed) {
			info.save(tileset);
		}
	} catch (const std::exception& e) {
		result = { false, std::string("Failed to rewrite asset references in war3map.w3i: ") + e.what() };
	}

	hierarchy.map_directory = original_map_directory;
	return result;
}

/// Rewrites Texture::file_name entries directly inside a raw .mdx buffer's TEXS chunk, without
/// parsing or re-serializing anything else in the file. Texture is a fixed 268-byte record
/// (replaceable_id: u32, file_name: 260-byte null-padded buffer, flags: u32 - see read_TEXS()/
/// write_TEXS() in mdx_reader.cpp/mdx_writer.cpp), so the rename is a pure byte-level find/replace:
/// locate the top-level "TEXS" chunk by walking chunk headers (tag + u32 size, like every other
/// top-level MDX chunk), then for each fixed-size entry, read the null-terminated name out of its
/// 260-byte slot, check it against the candidate table, and if it matches, zero the slot and write
/// the new (always much shorter) name back into it. Every other byte in the file - every other
/// chunk, and anything about TEXS this pipeline doesn't otherwise care about - is left untouched.
///
/// Added after discovering, via a real-map diagnostic dump, that MDX::to_mdx() does not round-trip
/// production models byte-for-byte even with zero content changes: confirmed causes include write_GEOS()
/// only emitting the TANG/SKIN chunk headers when the vector is non-empty (`if (geoset.tangents.size())`/
/// `if (geoset.skin.size())` in mdx_writer.cpp), which silently drops an originally-present-but-empty
/// TANG/SKIN chunk's 8-byte header on any save, plus MDX::validate()'s zero-extent "fix-up" and
/// unpreserved trailing bytes after a fixed string buffer's null terminator. Real-world bisection
/// testing isolated an in-game crash to exactly the code path that used to force a full model
/// reserialization for a texture-only rename (renaming .blp/.tga touches nearly every model's TEXS
/// chunk) - this function exists specifically to avoid that reserialization for the common case.
bool rewrite_mdx_textures_in_place(std::string& bytes, const std::unordered_map<std::string, const RenameCandidate*>& candidates_by_match_key) {
	if (bytes.size() < 4 || bytes.compare(0, 4, "MDLX") != 0) {
		return false;
	}

	bool changed = false;
	size_t pos = 4;
	while (pos + 8 <= bytes.size()) {
		const std::string_view tag(bytes.data() + pos, 4);
		uint32_t chunk_size = 0;
		std::memcpy(&chunk_size, bytes.data() + pos + 4, 4);
		const size_t data_start = pos + 8;
		if (data_start + chunk_size > bytes.size()) {
			break; // malformed/truncated chunk table - bail without touching anything further
		}

		if (tag == "TEXS") {
			const size_t entry_end = data_start + chunk_size;
			for (size_t entry_pos = data_start; entry_pos + 268 <= entry_end; entry_pos += 268) {
				const size_t name_offset = entry_pos + 4; // skip replaceable_id (u32)
				const size_t terminator = bytes.find('\0', name_offset);
				const size_t name_length = std::min(terminator == std::string::npos ? size_t{ 260 } : terminator - name_offset, size_t{ 260 });
				const std::string current_name = bytes.substr(name_offset, name_length);

				const auto found = candidates_by_match_key.find(asset_match_key(current_name));
				if (found == candidates_by_match_key.end()) {
					continue;
				}
				const std::string new_name = found->second->new_relative_path.string();
				if (new_name.size() >= 260) {
					continue; // can't happen for a generated hex name, but never overrun the fixed slot
				}
				bytes.replace(name_offset, 260, std::string(260, '\0'));
				bytes.replace(name_offset, new_name.size(), new_name);
				changed = true;
			}
		}

		pos = data_start + chunk_size;
	}

	return changed;
}

/// Rewrites paths stored *inside* MDX model files themselves - a model's own texture list
/// (Texture::file_name, handled surgically by rewrite_mdx_textures_in_place() above), attachment
/// paths (Attachment::path), and Emitter1 particle paths (ParticleEmitter1::path) are literal path
/// strings independent of whatever file the model itself was renamed to. Reads/writes temp_dir files
/// directly via plain file I/O rather than through Hierarchy, since every .mdx worth checking here is
/// already a loose file physically present in temp_dir (guaranteed by enumerate_rename_candidates()'s
/// walk) - no override/CASC lookup needed.
///
/// Only falls back to a full MDX::to_mdx() reserialization (with its known round-trip fidelity gaps -
/// see rewrite_mdx_textures_in_place()'s comment) when an attachment or particle path actually needs
/// rewriting, which needs the full structured parse since Attachment/ParticleEmitter1 records are
/// variable-length (interleaved animation tracks) and can't be located by fixed offset the way TEXS
/// entries can. That's a strictly rarer case than texture renames (it needs another *.mdx to itself be
/// a rename candidate, referenced as an attachment/particle model) and was not implicated by real-world
/// bisection testing, so accepting its residual round-trip risk here is a deliberate, narrower
/// trade-off than doing it for every model with any texture at all.
///
/// Skips (does not touch) any file that doesn't start with the "MDLX" magic - found via the same
/// real-map diagnostic: a same-named-but-different-extension text-format .MDL file was previously
/// being matched by this function's `.ends_with(".mdx")` filter (asset_match_key() normalizes .mdl to
/// .mdx for *candidate matching* purposes, but that normalization was leaking into "which files this
/// function treats as binary MDX to parse"), silently parsed as if it were binary MDX (failing the
/// magic check inside MDX::load() but not stopping there), and reserialized into a near-empty shell.
export AssetObfuscationResult rewrite_mdx_references(const fs::path& temp_dir, const std::vector<RenameCandidate>& candidates) {
	if (candidates.empty()) {
		return { true, "" };
	}

	std::unordered_map<std::string, const RenameCandidate*> candidates_by_match_key;
	for (const RenameCandidate& candidate : candidates) {
		candidates_by_match_key[candidate.match_key] = &candidate;
	}

	const auto rewrite_if_match = [&](std::string& value) {
		const auto found = candidates_by_match_key.find(asset_match_key(value));
		if (found != candidates_by_match_key.end()) {
			value = found->second->new_relative_path.string();
			return true;
		}
		return false;
	};

	if (!fs::exists(temp_dir)) {
		return { true, "" };
	}

	for (const auto& entry : fs::recursive_directory_iterator(temp_dir)) {
		if (!entry.is_regular_file() || !asset_match_key(entry.path().filename().string()).ends_with(".mdx")) {
			continue;
		}

		try {
			std::string bytes;
			{
				std::ifstream in(entry.path(), std::ios::binary);
				bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
			}

			if (bytes.size() < 4 || bytes.compare(0, 4, "MDLX") != 0) {
				continue; // not a binary MDX file (e.g. a same-named .MDL text file) - leave it alone
			}

			const bool texture_patched = rewrite_mdx_textures_in_place(bytes, candidates_by_match_key);
			if (texture_patched) {
				std::ofstream out(entry.path(), std::ios::binary | std::ios::trunc);
				out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
			}

			// Attachment/particle paths still need the full structured parse - re-read from disk so
			// this sees the texture patch above if one was just applied.
			auto file_result = read_file(entry.path());
			if (!file_result) {
				return { false, std::format("Failed to read '{}': {}", entry.path().filename().string(), file_result.error()) };
			}
			mdx::MDX model(file_result.value());

			bool other_changed = false;
			for (auto& attachment : model.attachments) {
				other_changed |= rewrite_if_match(attachment.path);
			}
			for (auto& emitter : model.emitters1) {
				other_changed |= rewrite_if_match(emitter.path);
			}

			if (other_changed) {
				const BinaryWriter writer = model.to_mdx(model.version);
				std::ofstream out(entry.path(), std::ios::binary | std::ios::trunc);
				out.write(reinterpret_cast<const char*>(writer.buffer.data()), static_cast<std::streamsize>(writer.buffer.size()));
			}
		} catch (const std::exception& e) {
			return { false, std::format("Failed to rewrite asset references in '{}': {}", entry.path().filename().string(), e.what()) };
		}
	}

	return { true, "" };
}

/// Rewrites path references inside .toc (table-of-contents) files - a plain-text format where each
/// non-empty line is a path to a .fdf UI-template file, used by BlzLoadTOCFile() to batch-load a set
/// of custom frame definitions in one call (e.g. a map's custom UI system lists its own CodeEditor.fdf,
/// CodeEditorButton.fdf, etc. alongside stock UI\FrameDef\... paths in one .toc file). Found via a
/// real map: nothing else in this pipeline understands the .toc format, so a renamed .fdf listed
/// inside one would go stale - BlzLoadTOCFile silently skips a line it can't resolve, so the failure
/// doesn't surface until later and confusingly, when whatever that .fdf defined fails to load (that
/// map's own script even had a dedicated error message anticipating exactly this: "Missing import:
/// CodeEditor.fdf.", which is coincidentally what tripped verify_no_dangling_text_references before
/// .toc handling existed - the message text, not an actual reference, since nothing rewrote or
/// scanned .toc files at the time).
export AssetObfuscationResult rewrite_toc_references(const fs::path& temp_dir, const std::vector<RenameCandidate>& candidates) {
	if (candidates.empty() || !fs::exists(temp_dir)) {
		return { true, "" };
	}

	std::unordered_map<std::string, const RenameCandidate*> candidates_by_match_key;
	for (const RenameCandidate& candidate : candidates) {
		candidates_by_match_key[candidate.match_key] = &candidate;
	}

	for (const auto& entry : fs::recursive_directory_iterator(temp_dir)) {
		if (!entry.is_regular_file() || !asset_match_key(entry.path().filename().string()).ends_with(".toc")) {
			continue;
		}

		std::string text;
		{
			std::ifstream in(entry.path(), std::ios::binary);
			text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		}

		std::vector<std::string> lines = absl::StrSplit(text, '\n');
		bool changed = false;
		for (std::string& line : lines) {
			const std::string_view content = trimmed(line);
			if (content.empty()) {
				continue;
			}
			const auto found = candidates_by_match_key.find(asset_match_key(std::string(content)));
			if (found == candidates_by_match_key.end()) {
				continue;
			}
			const bool has_cr = line.ends_with('\r');
			line = found->second->new_relative_path.string();
			if (has_cr) {
				line += '\r';
			}
			changed = true;
		}

		if (changed) {
			const std::string rebuilt = absl::StrJoin(lines, "\n");
			std::ofstream out(entry.path(), std::ios::binary | std::ios::trunc);
			out.write(rebuilt.data(), static_cast<std::streamsize>(rebuilt.size()));
		}
	}

	return { true, "" };
}

/// Rewrites path references inside .fdf (frame definition) files - WC3's UI-layout text format,
/// used for custom-skinned health bars, portraits, minimap borders, buttons, etc. A texture path
/// appears as an ordinary double-quoted string literal after a token like BackdropBackground/
/// BackdropEdgeFile/HighlightAlphaFile, indistinguishable at the syntax level from any other quoted
/// FDF string (a frame type, a frame name, a flag list) - so this reuses the same JASS/Lua-style
/// quoted-literal scanner already built for script asset references and TRIGSTR inlining
/// (find_string_literals(), with allow_single_quote_strings=false since FDF has no single-quoted
/// string form to worry about misclassifying) rather than trying to parse FDF's block grammar: any
/// literal whose *entire* unescaped content matches a candidate gets rewritten, and anything else -
/// keywords, frame names, flag strings - is left alone since those never happen to equal a
/// candidate's generated hex name.
///
/// Found via a real map: none of the other rewrite phases understand .fdf at all, so a renamed
/// texture referenced only from inside a custom .fdf (as opposed to also being reachable from
/// object data, war3map.w3i, or the script) went dangling - it didn't fail the export outright since
/// verify_no_dangling_text_references() didn't scan .fdf either at the time, so the game just showed
/// the affected frame as a missing-texture green box instead of the file failing to load entirely
/// (unlike a bad .toc reference, BlzFrameSetTexture-equivalent FDF fields don't have a dedicated
/// error message pointing back at the cause).
export AssetObfuscationResult rewrite_fdf_references(const fs::path& temp_dir, const std::vector<RenameCandidate>& candidates) {
	if (candidates.empty() || !fs::exists(temp_dir)) {
		return { true, "" };
	}

	std::unordered_map<std::string, const RenameCandidate*> candidates_by_match_key;
	for (const RenameCandidate& candidate : candidates) {
		candidates_by_match_key[candidate.match_key] = &candidate;
	}

	for (const auto& entry : fs::recursive_directory_iterator(temp_dir)) {
		if (!entry.is_regular_file() || !asset_match_key(entry.path().filename().string()).ends_with(".fdf")) {
			continue;
		}

		std::string text;
		{
			std::ifstream in(entry.path(), std::ios::binary);
			text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		}

		const std::vector<StringLiteralSpan> spans = find_string_literals(text, false);

		std::string rebuilt;
		rebuilt.reserve(text.size());
		size_t cursor = 0;
		bool changed = false;
		for (const StringLiteralSpan& span : spans) {
			const auto found = candidates_by_match_key.find(asset_match_key(unescape_script_string(span.raw_content)));
			if (found == candidates_by_match_key.end()) {
				continue;
			}

			rebuilt.append(text, cursor, span.start - cursor);
			rebuilt += '"';
			rebuilt += escape_script_string(found->second->new_relative_path.string());
			rebuilt += '"';
			cursor = span.end;
			changed = true;
		}
		rebuilt.append(text, cursor, text.size() - cursor);

		if (changed) {
			std::ofstream out(entry.path(), std::ios::binary | std::ios::trunc);
			out.write(rebuilt.data(), static_cast<std::streamsize>(rebuilt.size()));
		}
	}

	return { true, "" };
}

/// Rewrites path references in war3mapSkin.txt's [CustomSkin] section - the mechanism WC3 uses to
/// re-skin *stock* UI frames (health bar, resource icons, hero attribute icons, etc.) without needing
/// a custom .fdf at all: each line is `FrameName=path` (or `FrameName=NONE` for "no override"), e.g.
/// `GoldIcon=UI/NCOW_UI_IconCoin.tga`. war3mapSkin.txt's own filename is (correctly) never a rename
/// candidate - the engine looks it up by that exact hardcoded name, same as war3map.w3i - but nothing
/// previously rewrote its *contents*, so once a texture it names got renamed, the override just
/// pointed at nothing: found via a real map where every stock-frame override (gold/lumber icons, hero
/// attribute icons) turned into a green missing-texture box in-game despite the underlying files
/// being renamed and tracked as candidates correctly. Doesn't need to distinguish [CustomSkin] from
/// the [FrameDef] section below it (TRIGSTR_XXX string-table references, not paths) - a value is only
/// ever rewritten if it actually matches a candidate's match_key, which a TRIGSTR reference never
/// will, so a single generic `key=value` scan over the whole file is sufficient and section-agnostic.
export AssetObfuscationResult rewrite_war3mapskin_references(const fs::path& temp_dir, const std::vector<RenameCandidate>& candidates) {
	const fs::path skin_path = temp_dir / "war3mapSkin.txt";
	if (candidates.empty() || !fs::exists(skin_path)) {
		return { true, "" };
	}

	std::unordered_map<std::string, const RenameCandidate*> candidates_by_match_key;
	for (const RenameCandidate& candidate : candidates) {
		candidates_by_match_key[candidate.match_key] = &candidate;
	}

	std::string text;
	{
		std::ifstream in(skin_path, std::ios::binary);
		text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}

	std::vector<std::string> lines = absl::StrSplit(text, '\n');
	bool changed = false;
	for (std::string& line : lines) {
		const bool has_cr = line.ends_with('\r');
		const std::string_view line_view = has_cr ? std::string_view(line).substr(0, line.size() - 1) : std::string_view(line);
		const size_t equals = line_view.find('=');
		if (equals == std::string_view::npos) {
			continue;
		}
		const std::string_view key = line_view.substr(0, equals);
		const std::string_view value = line_view.substr(equals + 1);

		const auto found = candidates_by_match_key.find(asset_match_key(std::string(value)));
		if (found == candidates_by_match_key.end()) {
			continue;
		}

		line = std::string(key) + "=" + found->second->new_relative_path.string();
		if (has_cr) {
			line += '\r';
		}
		changed = true;
	}

	if (changed) {
		const std::string rebuilt = absl::StrJoin(lines, "\n");
		std::ofstream out(skin_path, std::ios::binary | std::ios::trunc);
		out.write(rebuilt.data(), static_cast<std::streamsize>(rebuilt.size()));
	}

	return { true, "" };
}

/// Safety-net scan run after all the structured rewrite phases (object data, war3map.w3i, MDX-
/// internal paths, script literals) have already run: catches a reference living somewhere none of
/// those phases correctly rewrote it. war3mapSkin.txt's own filename is (correctly) never a rename
/// candidate - the engine looks it up by that exact hardcoded name - but its *contents* are now
/// rewritten by rewrite_war3mapskin_references() above, so this scan (which also covers plain .txt
/// files generically) is what would catch a war3mapSkin.txt reference that rewrite phase missed for
/// some other reason. Also, in practice, a real gap found in rewrite_object_data_file() itself: a
/// real map's Ability Buff SFX fields (targetart/missileart/specialart/buffart in war3map.w3h) went
/// unrewritten for a reason not yet root-caused (the fields are genuine, manifest-tracked custom
/// imports - not a stock/desktop.ini-style false positive), and the resulting dangling references
/// crashed the game on load. That gap is why this scan also covers the object-data files, not just
/// text formats, despite the FunctionKind name.
///
/// Includes the object-data extensions (.w3u/.w3t/.w3a/.w3b/.w3h/.w3q/.w3d, plus their
/// war3mapSkin.* counterparts) alongside text formats (.txt/.ini/.j/.lua) - these files are
/// typically small (low hundreds of KB even on a large map), so scanning them costs little, unlike
/// the *actual* binary assets (BLP/MDX/WAV) this deliberately still excludes: those were already
/// exhaustively covered by the structured phases above (MDX contents specifically, by
/// rewrite_mdx_references()) or simply can't reference another file's path in the first place, so
/// scanning every byte of a many-MB import for no realistic gain stays out of scope. The matching
/// itself (contains_path_reference(), see below) is content-agnostic - it works identically whether
/// the file is text or binary, so widening this list costs nothing beyond the extra files read.
///
/// Also skips is_os_metadata_filename() files (desktop.ini) - found via a real case where one
/// referenced a renamed texture's old path (Windows Explorer folder-icon metadata, picked up
/// incidentally from the source folder), which the engine never reads regardless of content, so a
/// stale reference inside one isn't a real problem worth aborting the export over.
///
/// Matching goes through contains_path_reference(), not a plain substring search, for the same
/// reason: a real map's war3mapSkin.txt legitimately reads "blank-background.blp" for a UI element,
/// which a naive substring check flagged as referencing an unrelated candidate named "ground.blp"
/// (background.blp contains "...ground.blp" as a byte sequence purely by coincidence).
///
/// .j/.lua are the one exception to the contains_path_reference() substring scan above: a filename
/// can legitimately appear as a *fragment* of a much larger, human-readable string literal there (an
/// error/debug message that happens to mention an asset by name) without that string ever being used
/// as a load path - found via a real map whose custom code-editor UI throws
/// "Missing import: CodeEditor.fdf." if its .fdf failed to load, which a substring scan can't tell
/// apart from an actual reference. rewrite_script_asset_references() already rewrites every literal
/// whose *entire* unescaped content is exactly a candidate's path, so script files are instead
/// checked the same way here via find_string_literals() - only a whole literal matching a candidate
/// counts as a dangling reference, not a candidate's path merely appearing somewhere inside one.
export AssetObfuscationResult verify_no_dangling_text_references(const fs::path& temp_dir, const std::vector<RenameCandidate>& candidates) {
	if (candidates.empty() || !fs::exists(temp_dir)) {
		return { true, "" };
	}

	std::unordered_map<std::string, const RenameCandidate*> candidates_by_match_key;
	for (const RenameCandidate& candidate : candidates) {
		candidates_by_match_key[candidate.match_key] = &candidate;
	}

	const auto dangling_error = [](const std::string& file_name, const fs::path& original_relative_path) {
		return AssetObfuscationResult{
			false,
			std::format(
				"'{}' still references '{}', which is not a reference kind this pipeline rewrites - aborting rather "
				"than renaming a file something else still points at by its old name.",
				file_name, original_relative_path.string()
			)
		};
	};

	static constexpr std::array<std::string_view, 13> text_extensions = {
		".txt", ".ini", ".j", ".lua", ".w3u", ".w3t", ".w3a", ".w3b", ".w3h", ".w3q", ".w3d", ".toc", ".fdf",
	};

	for (const auto& entry : fs::recursive_directory_iterator(temp_dir)) {
		if (!entry.is_regular_file() || is_os_metadata_filename(entry.path().filename().string())) {
			continue;
		}
		std::string extension = entry.path().extension().string();
		std::ranges::transform(extension, extension.begin(), [](const unsigned char c) { return std::tolower(c); });
		if (!std::ranges::contains(text_extensions, extension)) {
			continue;
		}

		std::string content;
		{
			std::ifstream in(entry.path(), std::ios::binary);
			content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		}

		if (extension == ".j" || extension == ".lua" || extension == ".fdf") {
			// Whole-literal match, not a substring scan: a filename can legitimately appear as a
			// *fragment* of a larger, human-readable string (an error message, a frame/control name)
			// without that string being used as a load path - see rewrite_fdf_references()'s comment
			// for why .fdf specifically needs this same treatment as script literals.
			for (const StringLiteralSpan& span : find_string_literals(content, extension == ".lua")) {
				const auto found = candidates_by_match_key.find(asset_match_key(unescape_script_string(span.raw_content)));
				if (found != candidates_by_match_key.end()) {
					return dangling_error(entry.path().filename().string(), found->second->original_relative_path);
				}
			}
			continue;
		}

		std::ranges::transform(content, content.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });

		for (const RenameCandidate& candidate : candidates) {
			std::string backward_key = candidate.match_key;
			std::ranges::replace(backward_key, '/', '\\');
			if (contains_path_reference(content, candidate.match_key) || contains_path_reference(content, backward_key)) {
				return dangling_error(entry.path().filename().string(), candidate.original_relative_path);
			}
		}
	}

	return { true, "" };
}

/// Physically renames every candidate on disk. Must run last, strictly after every rewrite phase and
/// the verification scan above have already succeeded - every reference elsewhere in the map already
/// points at new_relative_path by this point, so renaming any earlier would make those references
/// (briefly, but for real if any step in between failed) point at nothing.
export AssetObfuscationResult apply_renames(const fs::path& temp_dir, const std::vector<RenameCandidate>& candidates) {
	for (const RenameCandidate& candidate : candidates) {
		std::error_code ec;
		fs::rename(temp_dir / candidate.original_relative_path, temp_dir / candidate.new_relative_path, ec);
		if (ec) {
			return {
				false,
				std::format(
					"Failed to rename '{}' to '{}': {}", candidate.original_relative_path.string(), candidate.new_relative_path.string(), ec.message()
				)
			};
		}
	}
	return { true, "" };
}
