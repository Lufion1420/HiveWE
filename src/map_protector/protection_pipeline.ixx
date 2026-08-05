module;

#define __STORMLIB_NO_STATIC_LINK__
#include "StormLib.h"
#include <filesystem>

export module ProtectionPipeline;

import std;
import Map;
import MapGlobal;
import Hierarchy;
import Imports;
import MapInfo;
import BinaryReader;
import MPQ;
import AssetObfuscation;

namespace fs = std::filesystem;

export struct ProtectionOptions {
	// MPQ archive hardening
	bool remove_listfile = true;
	bool remove_attributes = true;
	bool encrypt_files = true;
	bool inject_junk_files = false;
	int junk_file_count = 50;

	// Trigger hardening
	bool remove_gui_triggers = true;
	bool strip_trigger_strings = false;

	// Metadata sanitization
	bool clear_author = false;
	bool clear_description = false;
	bool clear_loading_text = false;
	bool normalize_name = false;

	// Asset path obfuscation - plumbing only so far (see run_async_pack()); enumerate_rename_candidates()
	// runs to validate the module boundary, but no file is actually renamed or has its references
	// rewritten yet, so enabling this currently just fails the export with a clear message rather
	// than silently doing nothing or shipping a map with dangling references.
	bool obfuscate_asset_paths = false;
};

export struct SyncSaveResult {
	bool success = false;
	std::string error;
};

export struct PackResult {
	bool success = false;
	std::string error;
};

/// Saves the currently loaded map into temp_dir with the requested metadata fields cleared.
///
/// This is the only function in the pipeline allowed to touch the global `map`/`hierarchy`
/// singletons. Map::save() mutates map->filesystem_path/map->name when the target path differs
/// from the map's own folder, and its sub-steps (info.save(), triggers.save(), etc.) resolve
/// their write locations through hierarchy.map_directory rather than the path passed to save() -
/// see HiveWE::save_current_map_as() for the existing precedent of redirecting map_directory
/// before a save to a different location. Both are redirected here and restored immediately and
/// unconditionally once save() returns, so no other code can observe the redirected state.
/// Must be called on the UI thread, synchronously, before any background work starts.
export SyncSaveResult run_sync_save_and_restore(const fs::path& temp_dir, const ProtectionOptions& options) {
	if (!map || !map->loaded) {
		return { false, "No map is loaded." };
	}

	const fs::path original_filesystem_path = map->filesystem_path;
	const std::string original_name = map->name;
	const fs::path original_map_directory = hierarchy.map_directory;

	const std::string original_author = map->info.author;
	const std::string original_description = map->info.description;
	const std::string original_loading_screen_text = map->info.loading_screen_text;
	const std::string original_loading_screen_title = map->info.loading_screen_title;
	const std::string original_loading_screen_subtitle = map->info.loading_screen_subtitle;
	const std::string original_info_name = map->info.name;

	if (options.clear_author) {
		map->info.author.clear();
	}
	if (options.clear_description) {
		map->info.description.clear();
	}
	if (options.clear_loading_text) {
		map->info.loading_screen_text.clear();
		map->info.loading_screen_title.clear();
		map->info.loading_screen_subtitle.clear();
	}
	if (options.normalize_name) {
		map->info.name = "Warcraft III Map";
	}

	hierarchy.map_directory = temp_dir;
	const bool save_ok = map->save(temp_dir);

	// Restore immediately and unconditionally - no early return above this point.
	map->filesystem_path = original_filesystem_path;
	map->name = original_name;
	hierarchy.map_directory = original_map_directory;
	map->info.author = original_author;
	map->info.description = original_description;
	map->info.loading_screen_text = original_loading_screen_text;
	map->info.loading_screen_title = original_loading_screen_title;
	map->info.loading_screen_subtitle = original_loading_screen_subtitle;
	map->info.name = original_info_name;

	if (!save_ok) {
		return { false, "Failed to save map data to a temporary folder." };
	}
	return { true, "" };
}

/// Copies a folder-mode map's loose files into temp_dir untouched. Unlike
/// run_sync_save_and_restore(), this never loads the map into HiveWE's Map object, so files
/// HiveWE doesn't fully understand (e.g. an externally compiled war3map.lua) pass through
/// byte-for-byte instead of being regenerated from HiveWE's own state.
SyncSaveResult copy_source_folder(const fs::path& source_directory, const fs::path& temp_dir) {
	if (!fs::exists(source_directory / "war3map.w3i")) {
		return { false, "The selected folder is not a Warcraft III map (no war3map.w3i)." };
	}

	std::error_code ec;
	fs::copy(source_directory, temp_dir, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
	if (ec) {
		return { false, "Failed to copy the source map: " + ec.message() };
	}
	return { true, "" };
}

/// Unpacks a .w3x/.w3m archive into temp_dir. Mirrors HiveWE::load_mpq(): wildcard enumeration
/// depends on the archive's (listfile) and loses real names when it's absent, so the fixed set
/// of core map files is also fetched directly by name, which works regardless of listfile
/// presence since MPQ files are looked up by name hash.
SyncSaveResult unpack_source_archive(const fs::path& source_file, const fs::path& temp_dir) {
	mpq::MPQ mpq;
	if (!mpq.open(source_file)) {
		return { false, "Failed to open the archive. It might be open in another program." };
	}

	if (!mpq.unpack(temp_dir)) {
		return { false, "Failed to unpack the archive." };
	}

	static const Imports imports;
	for (const std::string& name : imports.blacklist) {
		mpq.extract_file(name, temp_dir / name);
	}

	if (!fs::exists(temp_dir / "war3map.w3i")) {
		return { false, "The archive does not contain a valid Warcraft III map (war3map.w3i is missing)." };
	}
	return { true, "" };
}

/// Rewrites war3map.w3i's metadata fields under temp_dir in isolation. MapInfo only depends on
/// Hierarchy, not the rest of Map, so this avoids loading/regenerating terrain, triggers, or the
/// map script - unlike run_sync_save_and_restore(). MapInfo::save() needs a tileset byte that
/// MapInfo::load() itself discards, so it's read directly from war3map.w3e's header instead
/// (same layout Terrain::load() parses: 4-byte "W3E!" magic, 4-byte version, 1-byte tileset).
SyncSaveResult sanitize_metadata(const fs::path& temp_dir, const ProtectionOptions& options) {
	if (!options.clear_author && !options.clear_description && !options.clear_loading_text && !options.normalize_name) {
		return { true, "" };
	}

	const fs::path original_map_directory = hierarchy.map_directory;
	hierarchy.map_directory = temp_dir;

	char tileset = 'L';
	if (auto w3e = hierarchy.map_file_read("war3map.w3e"); w3e && w3e->read_string(4) == "W3E!") {
		w3e->advance(4); // format version
		tileset = w3e->read<char>();
	}

	SyncSaveResult result{ true, "" };
	try {
		MapInfo info;
		info.load();
		if (options.clear_author) {
			info.author.clear();
		}
		if (options.clear_description) {
			info.description.clear();
		}
		if (options.clear_loading_text) {
			info.loading_screen_text.clear();
			info.loading_screen_title.clear();
			info.loading_screen_subtitle.clear();
		}
		if (options.normalize_name) {
			info.name = "Warcraft III Map";
		}
		info.save(tileset);
	} catch (const std::exception& e) {
		result = { false, std::string("Failed to sanitize map metadata: ") + e.what() };
	}

	hierarchy.map_directory = original_map_directory;
	return result;
}

/// Entry point for protecting an external map file/folder, bypassing Map::load()/save()
/// entirely. Must be called on the UI thread, synchronously, before any background work starts -
/// sanitize_metadata() briefly redirects the global hierarchy.map_directory, same constraint as
/// run_sync_save_and_restore().
export SyncSaveResult prepare_source_path(const fs::path& source, const fs::path& temp_dir, const ProtectionOptions& options) {
	std::error_code ec;
	const bool is_dir = fs::is_directory(source, ec);

	SyncSaveResult result = is_dir ? copy_source_folder(source, temp_dir) : unpack_source_archive(source, temp_dir);
	if (!result.success) {
		return result;
	}

	return sanitize_metadata(temp_dir, options);
}

/// Adds `count` files with random names, extensions, and content to temp_dir so they get
/// packed into the archive alongside the real map files. Purely cosmetic noise for anyone
/// enumerating archive contents - WC3 only loads files that are actually referenced by name,
/// so unreferenced junk is silently ignored by the game.
void inject_junk_files(const fs::path& temp_dir, int count) {
	static constexpr std::array<std::string_view, 5> junk_extensions = { ".blp", ".mdx", ".wav", ".dat", ".txt" };

	std::mt19937 rng{ std::random_device{}() };
	std::uniform_int_distribution<int> hex_digit(0, 15);
	std::uniform_int_distribution<int> extension_index(0, static_cast<int>(junk_extensions.size()) - 1);
	std::uniform_int_distribution<int> content_size(64, 2048);
	std::uniform_int_distribution<int> content_byte(0, 255);

	for (int i = 0; i < count; ++i) {
		std::string name = "_junk_";
		for (int digit = 0; digit < 8; ++digit) {
			name += "0123456789abcdef"[hex_digit(rng)];
		}
		name += junk_extensions[extension_index(rng)];

		std::vector<char> content(content_size(rng));
		for (char& byte : content) {
			byte = static_cast<char>(content_byte(rng));
		}

		std::ofstream file(temp_dir / name, std::ios::binary);
		file.write(content.data(), static_cast<std::streamsize>(content.size()));
	}
}

/// Reads temp_dir/war3map.wts directly off disk and parses it into id -> text, independent of
/// the TriggerStrings class (which reads through the global hierarchy) so this stays a plain,
/// background-thread-safe file operation. Mirrors TriggerStrings::load()'s parsing exactly,
/// including its zero-pad-to-3-digits key convention, since callers match against that format.
std::unordered_map<std::string, std::string> parse_trigger_strings_file(const fs::path& wts_path) {
	std::unordered_map<std::string, std::string> table;

	std::ifstream stream(wts_path, std::ios::binary);
	if (!stream) {
		return table;
	}
	std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

	// Skip a leading UTF-8 BOM, which TriggerStrings::save() always writes.
	if (contents.starts_with("\xEF\xBB\xBF")) {
		contents.erase(0, 3);
	}

	std::istringstream file(contents);
	std::string key;
	std::string line;
	while (std::getline(file, line)) {
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		if (line.empty() || line.starts_with("//")) {
			continue;
		}

		if (line.front() == '{') {
			std::string value;
			bool first = true;
			while (std::getline(file, line) && !line.empty() && line.front() != '}') {
				if (!line.empty() && line.back() == '\r') {
					line.pop_back();
				}
				value += (first ? "" : "\n") + line;
				first = false;
			}
			table[key] = value;
		} else {
			const size_t found = line.find(' ') + 1;
			if (found == std::string::npos || found >= line.size()) {
				continue;
			}
			const int padsize = std::max(0, 3 - (static_cast<int>(line.size()) - static_cast<int>(found)));
			key = "TRIGSTR_" + std::string(padsize, '0') + line.substr(found);
		}
	}
	return table;
}

/// Escapes text for embedding as the body of a JASS or Lua double-quoted string literal. Both
/// languages accept the same core escapes (\\, \", \n) for this purpose. Control bytes other
/// than newline are dropped rather than risking an escape sequence the game's script parser
/// doesn't accept - trigger string text is always printable text in practice (WC3 itself uses
/// the literal 2-character sequence "|n", not a real newline byte, for tooltip line breaks).
std::string escape_script_string(const std::string& text) {
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

/// If `literal` (a string literal's raw, still-escaped content as it appears in source) is a
/// trigger string reference, returns its canonical zero-padded key (e.g. "TRIGSTR_007").
/// WC3 recognizes a string value as a trigger string reference if it *starts* with "TRIGSTR_"
/// followed by digits - trailing characters after the digits are accepted but ignored by the
/// engine (community-documented: "TRIGSTR_7abc" still resolves to trigger string #7) - so this
/// matches on the same prefix+digits rule rather than requiring an exact whole-string match.
std::optional<std::string> trigstr_key_from_literal(const std::string& literal) {
	constexpr std::string_view prefix = "TRIGSTR_";
	if (!literal.starts_with(prefix)) {
		return std::nullopt;
	}

	size_t i = prefix.size();
	const size_t digits_start = i;
	while (i < literal.size() && std::isdigit(static_cast<unsigned char>(literal[i]))) {
		++i;
	}
	if (i == digits_start) {
		return std::nullopt;
	}

	std::string digits = literal.substr(digits_start, i - digits_start);
	const size_t first_nonzero = digits.find_first_not_of('0');
	digits = (first_nonzero == std::string::npos) ? "0" : digits.substr(first_nonzero);
	if (digits.size() < 3) {
		digits.insert(0, 3 - digits.size(), '0');
	}
	return "TRIGSTR_" + digits;
}

struct StringLiteralSpan {
	size_t start; // index of the opening quote
	size_t end; // index one past the closing quote
	std::string raw_content; // between the quotes, still escaped as it appears in source
};

/// Scans `text` (a JASS or Lua source file's full contents) for double-quoted string literals,
/// skipping both languages' line/block comment styles ("//", "/* */", "--", "--[[ ]]") so a
/// quote character inside a comment can't desynchronize the scan for everything after it.
/// Respects \\ and \" so an escaped quote doesn't end a literal early.
std::vector<StringLiteralSpan> find_string_literals(const std::string& text) {
	std::vector<StringLiteralSpan> spans;
	size_t i = 0;
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
			const size_t start = i;
			std::string content;
			++i;
			bool terminated = false;
			while (i < text.size() && text[i] != '\n') {
				if (text[i] == '"') {
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
			// Unterminated literal (shouldn't happen in valid generated script): leave it out of
			// the results rather than guessing where it ends.
		} else {
			++i;
		}
	}
	return spans;
}

/// Resolves TRIGSTR references in war3map.w3i's own text fields (name, author, description,
/// loading screen text/title/subtitle) and rewrites the file if anything changed. These are plain
/// strings that can themselves literally read "TRIGSTR_XXX" - the World Editor writes that when a
/// field is set via a localized/custom-text string picker, which is the common case for the
/// loading screen fields and sometimes the map name. Left unpatched, they go dangling the instant
/// war3map.wts is deleted: the map name and/or loading screen text then show up blank in-game,
/// even with every "Metadata Sanitization" checkbox left off, since that's a separate code path.
/// Fails (does not delete war3map.wts) if a referenced key has no entry in the wts table, mirroring
/// the script-patching behavior below - a dangling reference here is unrecoverable once wts is gone.
SyncSaveResult inline_map_info_trigger_strings(const fs::path& temp_dir, const std::unordered_map<std::string, std::string>& trigger_string_table) {
	if (!fs::exists(temp_dir / "war3map.w3i")) {
		return { true, "" };
	}

	const fs::path original_map_directory = hierarchy.map_directory;
	hierarchy.map_directory = temp_dir;

	char tileset = 'L';
	if (auto w3e = hierarchy.map_file_read("war3map.w3e"); w3e && w3e->read_string(4) == "W3E!") {
		w3e->advance(4); // format version
		tileset = w3e->read<char>();
	}

	SyncSaveResult result{ true, "" };
	try {
		MapInfo info;
		info.load();

		bool changed = false;
		std::string unresolved_key;
		auto resolve = [&](std::string& field) -> bool {
			const std::optional<std::string> key = trigstr_key_from_literal(field);
			if (!key) {
				return true;
			}
			const auto found = trigger_string_table.find(*key);
			if (found == trigger_string_table.end()) {
				unresolved_key = *key;
				return false;
			}
			field = found->second;
			changed = true;
			return true;
		};

		const bool ok = resolve(info.name) && resolve(info.author) && resolve(info.description)
			&& resolve(info.loading_screen_text) && resolve(info.loading_screen_title) && resolve(info.loading_screen_subtitle);

		if (!ok) {
			result = { false, std::format("war3map.w3i references {} which has no entry in war3map.wts - aborting rather than shipping a map with unresolved map name/loading screen text.", unresolved_key) };
		} else if (changed) {
			info.save(tileset);
		}
	} catch (const std::exception& e) {
		result = { false, std::string("Failed to inline trigger strings into war3map.w3i: ") + e.what() };
	}

	hierarchy.map_directory = original_map_directory;
	return result;
}

/// Replaces every trigger-string-reference literal in war3map.j/war3map.lua (whichever are
/// present under temp_dir) with the resolved, escaped text from war3map.wts, then inlines any
/// TRIGSTR references in war3map.w3i's own metadata fields (map name / loading screen text), then
/// deletes war3map.wts. Fails the whole step - callers should abort the export rather than ship a
/// partially-resolved script - if any matched reference has no corresponding entry in the wts
/// table, since silently leaving a raw "TRIGSTR_007" literal behind means that text is gone
/// forever once war3map.wts is deleted.
///
/// Scope note: this covers the script and war3map.w3i's own fields. Object data fields (unit/item/
/// ability tooltips etc.) can independently store a TRIGSTR reference too, but patching those would
/// require loading the full SLK/meta tables this pipeline's external-source path deliberately
/// avoids touching. If a map stores long custom object-data text this way, that field will show
/// raw "TRIGSTR_XXX" text in-game after stripping - a disclosed, known limitation, not a bug.
SyncSaveResult strip_trigger_strings_step(const fs::path& temp_dir) {
	const fs::path wts_path = temp_dir / "war3map.wts";
	if (!fs::exists(wts_path)) {
		return { true, "" };
	}

	const std::unordered_map<std::string, std::string> trigger_string_table = parse_trigger_strings_file(wts_path);

	for (const char* script_name : { "war3map.j", "war3map.lua" }) {
		const fs::path script_path = temp_dir / script_name;
		if (!fs::exists(script_path)) {
			continue;
		}

		std::string text;
		{
			std::ifstream in(script_path, std::ios::binary);
			text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		}

		const std::vector<StringLiteralSpan> spans = find_string_literals(text);

		std::string rebuilt;
		rebuilt.reserve(text.size());
		size_t cursor = 0;
		for (const StringLiteralSpan& span : spans) {
			const std::optional<std::string> key = trigstr_key_from_literal(span.raw_content);
			if (!key) {
				continue;
			}

			const auto found = trigger_string_table.find(*key);
			if (found == trigger_string_table.end()) {
				return { false, std::format("'{}' references {} which has no entry in war3map.wts - aborting rather than shipping a script with an unresolved reference.", script_name, *key) };
			}

			rebuilt.append(text, cursor, span.start - cursor);
			rebuilt += '"';
			rebuilt += escape_script_string(found->second);
			rebuilt += '"';
			cursor = span.end;
		}
		rebuilt.append(text, cursor, text.size() - cursor);

		std::ofstream out(script_path, std::ios::binary | std::ios::trunc);
		out.write(rebuilt.data(), static_cast<std::streamsize>(rebuilt.size()));
	}

	const SyncSaveResult info_result = inline_map_info_trigger_strings(temp_dir, trigger_string_table);
	if (!info_result.success) {
		return info_result;
	}

	std::error_code ec;
	fs::remove(wts_path, ec);
	return { true, "" };
}

/// Packs temp_dir into a protected MPQ at output_path. Operates only on plain files under
/// temp_dir/output_path - never touches map/hierarchy - so it is safe to run on a background
/// thread once run_sync_save_and_restore() has returned. Mirrors HiveWE::export_mpq()'s raw
/// StormLib usage; the MPQ wrapper in mpq.ixx has no archive-creation support.
export PackResult run_async_pack(const fs::path& temp_dir, const fs::path& output_path, const ProtectionOptions& options) {
	if (options.obfuscate_asset_paths) {
		// Candidate enumeration is real and exercised here to prove the module boundary works end
		// to end, but nothing downstream (SLK/w3i/MDX/script reference rewriting) exists yet, so
		// renaming these files now would ship a map with dangling references. Fail loudly instead
		// of either silently doing nothing or silently breaking the map - re-enable once the
		// reference-rewrite phases land (see .cursor/plans/map_protection_plan.md).
		static const Imports imports;
		const std::vector<RenameCandidate> candidates = enumerate_rename_candidates(
			temp_dir, imports.blacklist, [](const fs::path& path) { return hierarchy.game_file_exists(path); }
		);
		return {
			false,
			std::format(
				"Asset Path Obfuscation is not fully implemented yet ({} candidate file(s) found) - reference "
				"rewriting for object data, war3map.w3i, models, and scripts hasn't shipped. Leave this option off.",
				candidates.size()
			)
		};
	}

	if (options.remove_gui_triggers) {
		std::error_code ec;
		fs::remove(temp_dir / "war3map.wtg", ec);
	}

	if (options.strip_trigger_strings) {
		const SyncSaveResult strip_result = strip_trigger_strings_step(temp_dir);
		if (!strip_result.success) {
			return { false, strip_result.error };
		}
	}

	if (options.inject_junk_files && options.junk_file_count > 0) {
		inject_junk_files(temp_dir, options.junk_file_count);
	}

	std::error_code remove_ec;
	fs::remove(output_path, remove_ec);

	std::error_code dir_ec;
	fs::create_directories(output_path.parent_path(), dir_ec);

	const uint64_t file_count = std::distance(fs::recursive_directory_iterator{ temp_dir }, {});

	const unsigned long create_flags = (options.remove_listfile ? 0ul : static_cast<unsigned long>(MPQ_CREATE_LISTFILE))
		| (options.remove_attributes ? 0ul : static_cast<unsigned long>(MPQ_CREATE_ATTRIBUTES));

	HANDLE handle;
	if (!SFileCreateArchive(output_path.c_str(), create_flags, file_count, &handle)) {
		return { false, std::format("There was an error creating the protected archive (error code {}).", GetLastError()) };
	}

	const unsigned long file_flags = static_cast<unsigned long>(MPQ_FILE_COMPRESS)
		| (options.encrypt_files ? static_cast<unsigned long>(MPQ_FILE_ENCRYPTED) : 0ul);

	for (const auto& entry : fs::recursive_directory_iterator(temp_dir)) {
		if (entry.is_regular_file()) {
			if (!SFileAddFileEx(handle, entry.path().c_str(), entry.path().lexically_relative(temp_dir).string().c_str(), file_flags, MPQ_COMPRESSION_ZLIB, MPQ_COMPRESSION_NEXT_SAME)) {
				const DWORD add_file_error = GetLastError();
				SFileCloseArchive(handle);
				return { false, std::format("There was an error adding '{}' to the protected archive (error code {}).", entry.path().filename().string(), add_file_error) };
			}
		}
	}

	SFileCompactArchive(handle, nullptr, false);
	SFileCloseArchive(handle);

	return { true, "" };
}
